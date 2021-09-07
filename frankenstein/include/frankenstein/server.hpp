#pragma once

#include <memory>

// #include <grpcpp/alarm.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <proto/exchange_service.grpc.pb.h>

#include <frankenstein/commons.hpp>

namespace frankenstein {

using grpc_server_ptr = std::unique_ptr<::grpc::Server>;
using grpc_server_queue_ptr = std::shared_ptr<::grpc::ServerCompletionQueue>;
using grpc_server_context = ::grpc::ServerContext;

using async_service = proto::ExchangeService::AsyncService;
using async_service_ptr = std::shared_ptr<proto::ExchangeService::AsyncService>;

template <class T>
using server_async_writer = ::grpc::ServerAsyncWriter<T>;
template <class T>
using server_async_response_writer = ::grpc::ServerAsyncResponseWriter<T>;

// ---------------------------------------------------------------------------------------------------------------------

class server : public QObject
{
    Q_OBJECT

public:
    explicit server(uint16_t port);
    ~server() override;

    void init();

private slots:
    void stop();
    void poll();

private:
    const uint16_t _port;
    QTimer _timer;

    grpc_server_ptr _server;
    grpc_server_queue_ptr _queue;
    async_service_ptr _service;
};

// ---------------------------------------------------------------------------------------------------------------------

class server_method_handler_stub
{
public:
    virtual bool proceed(bool ok) = 0;
    virtual ~server_method_handler_stub() = default;
};

template <typename Writer, typename Service, typename Request, typename Reply>
class server_method_handler : public server_method_handler_stub
{
protected:
    using writer_type = Writer;
    using service_type = Service;
    using request_type = Request;
    using reply_type = Reply;

    grpc_server_queue_ptr _queue;
    grpc_server_context _context;

    writer_type _responder;
    service_type _service;
    request_type _request;
    reply_type _reply;

    server_method_handler(Service service, grpc_server_queue_ptr queue) :
        _queue(queue),
        _context(),
        _responder(&_context),
        _service(service)
    {}
    virtual ~server_method_handler() = default;
};

template <typename Service, typename Request, typename Reply>
class server_method_handler_oto :
    public server_method_handler<server_async_response_writer<Reply>, Service, Request, Reply>
{
public:
    server_method_handler_oto(Service service, grpc_server_queue_ptr queue) :
        server_method_handler<server_async_response_writer<Reply>, Service, Request, Reply>(service, queue)
    {}

    server_method_handler_oto(const server_method_handler_oto&) = delete;
    server_method_handler_oto& operator=(const server_method_handler_oto&) = delete;
    server_method_handler_oto(server_method_handler_oto&&) = delete;
    server_method_handler_oto& operator=(server_method_handler_oto&&) = delete;

protected:
    enum class handler_state
    {
        CREATE,
        PROCESS,
        FINISH
    };

    handler_state _state = handler_state::CREATE;
};

template <typename Service, typename Request, typename Reply>
class server_method_handler_otm : public server_method_handler<server_async_writer<Reply>, Service, Request, Reply>
{
public:
    server_method_handler_otm(Service service, grpc_server_queue_ptr queue) :
        server_method_handler<server_async_writer<Reply>, Service, Request, Reply>(service, queue)
    {}

    server_method_handler_otm(const server_method_handler_otm&) = delete;
    server_method_handler_otm& operator=(const server_method_handler_otm&) = delete;
    server_method_handler_otm(server_method_handler_otm&&) = delete;
    server_method_handler_otm& operator=(server_method_handler_otm&&) = delete;

    bool proceed(bool ok) override;

protected:
    virtual void handle_call_state() = 0;
    virtual void handle_wait_state() = 0;
    virtual void handle_write_state() = 0;

    enum class handler_state
    {
        CALL,
        WAIT,
        WRITE,
        FINISH
    };

    handler_state _state = handler_state::CALL;
};

template <typename S, typename Req, typename Rep>
bool server_method_handler_otm<S, Req, Rep>::proceed(bool ok)
{
    LOG("server_method_handler_otm::proceed()");

    if (_state == handler_state::FINISH) {
        LOG("server_method_handler_otm::proceed(): call finished");
        return false;
    }

    if (ok) {
        if (_state == handler_state::CALL)
            this->handle_call_state();
        else if (_state == handler_state::WAIT)
            this->handle_wait_state();
        else if (_state == handler_state::WRITE)
            this->handle_write_state();
    } else {
        if (_state == handler_state::WAIT) {
            // server has been shut down before receiving a matching DownloadRequest
            LOG("server_method_handler_otm::proceed(): server has been shut down before receiving a matching "
                "request");
            return false;
        } else {
            LOG("server_method_handler_otm::proceed(): abort: call is cancelled or connection is dropped");
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// one-to-one Handler

class ping_server_handler : public server_method_handler_oto<async_service_ptr, proto::EmptyRequest, proto::StringReply>
{
public:
    ping_server_handler(async_service_ptr service, grpc_server_queue_ptr queue);

    bool proceed(bool ok) override;
};

// ---------------------------------------------------------------------------------------------------------------------
// one-to-many handler

class stream_string_server_handler :
    public server_method_handler_otm<async_service_ptr, proto::NameRequest, proto::StringReply>
{
public:
    stream_string_server_handler(async_service_ptr service, grpc_server_queue_ptr queue);
    ~stream_string_server_handler() override;

private:
    void handle_call_state() override;
    void handle_wait_state() override;
    void handle_write_state() override;

    uint8_t amount = 5;
};

class stream_int_server_handler :
    public server_method_handler_otm<async_service_ptr, proto::NameRequest, proto::IntReply>
{
public:
    stream_int_server_handler(async_service_ptr service, grpc_server_queue_ptr queue);
    ~stream_int_server_handler() override;

private:
    void handle_call_state() override;
    void handle_wait_state() override;
    void handle_write_state() override;

    uint8_t amount = 5;
};

} // namespace frankenstein
