#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <proto/exchange_service.grpc.pb.h>

#include <frankenstein/commons.hpp>

namespace frankenstein {

using grpc_client_queue_ptr = std::shared_ptr<::grpc::CompletionQueue>;
using grpc_client_context = ::grpc::ClientContext;

using stub_ptr = std::shared_ptr<proto::ExchangeService::Stub>;

template <class T>
using client_async_response_reader = ::grpc::ClientAsyncResponseReader<T>;
template <class T>
using client_async_reader = ::grpc::ClientAsyncReader<T>;

// ---------------------------------------------------------------------------------------------------------------------

class client_method_handler_stub : public QObject
{
    Q_OBJECT

public:
    virtual bool proceed(bool ok) = 0;
    virtual ~client_method_handler_stub() = default;
};

template <typename Reader, typename Request, typename Reply>
class client_method_handler : public client_method_handler_stub
{
public:
    client_method_handler() = default;
    virtual ~client_method_handler() = default;

    void cancel();

protected:
    using reader_type = Reader;
    using request_type = Request;
    using reply_type = Reply;

    grpc_client_context _context;
    std::unique_ptr<reader_type> _reader;

    grpc_status _status;
    reply_type _reply;
};

template <typename Reader, typename Request, typename Reply>
void client_method_handler<Reader, Request, Reply>::cancel()
{
    LOG("client_method_handler::cancel()");
    _context.TryCancel();
}

// ---------------------------------------------------------------------------------------------------------------------
// one-to-one handler

template <typename Request, typename Reply>
class client_method_handler_oto : public client_method_handler<client_async_response_reader<Reply>, Request, Reply>
{
public:
    client_method_handler_oto() = default;
};

class ping_client_handler : public client_method_handler_oto<proto::EmptyRequest, proto::StringReply>
{
public:
    ping_client_handler(const request_type& request, stub_ptr stub, grpc_client_queue_ptr queue);
    ~ping_client_handler() override;

    bool proceed(bool ok) override;
};

// ---------------------------------------------------------------------------------------------------------------------
// one-to-many handler

template <typename Request, typename Reply>
class client_method_handler_otm : public client_method_handler<client_async_reader<Reply>, Request, Reply>
{
protected:
    using request_type = Request;

public:
    client_method_handler_otm(const request_type& request, stub_ptr stub, grpc_client_queue_ptr queue);

    bool proceed(bool ok) override;
    bool is_closed() const { return _state == handler_state::CLOSED; };

protected:
    virtual void handle_call_state() = 0;
    virtual void handle_write_state() = 0;
    virtual void handle_wait_state() = 0;
    virtual void handle_read_state() = 0;
    virtual void handle_finish_state() = 0;

    enum class handler_state
    {
        CALL,
        WRITE,
        WAIT,
        READ,
        FINISH,
        CLOSED
    };

    stub_ptr _stub;
    grpc_client_queue_ptr _queue;
    request_type _request;

    handler_state _state = handler_state::CALL;
};

template <typename Request, typename Reply>
client_method_handler_otm<Request, Reply>::client_method_handler_otm(const Request& request,
                                                                     stub_ptr stub,
                                                                     grpc_client_queue_ptr queue) :
    client_method_handler<client_async_reader<Reply>, Request, Reply>(),
    _stub(stub),
    _queue(queue),
    _request(request)
{}

template <typename Request, typename Reply>
bool client_method_handler_otm<Request, Reply>::proceed(bool ok)
{
    LOG("client_method_handler_otm::proceed()");

    try {
        if (ok) {
            if (_state == handler_state::CALL) {
                this->handle_call_state();
            } else if (_state == handler_state::WRITE) {
                this->handle_write_state();
            } else if (_state == handler_state::WAIT) {
                this->handle_wait_state();
            } else if (_state == handler_state::READ) {
                this->handle_read_state();
            } else if (_state == handler_state::FINISH) {
                this->handle_finish_state();
                return false;
            }
        } else {
            _state = handler_state::FINISH;
            this->_reader->Finish(&this->_status, this);
        }

        return true;
    } catch (std::exception& e) {
        LOG("client_method_handler_otm::proceed(): processing error: " + std::string(e.what()));
    } catch (...) {
        LOG("client_method_handler_otm::proceed(): processing error: unknown exception caught");
    }

    if (_state == handler_state::CALL) {
        LOG("client_method_handler_otm::proceed(): return after exception on state=CALL");
        return false;
    }

    this->_context.TryCancel();

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

class stream_string_client_handler : public client_method_handler_otm<proto::NameRequest, proto::StringReply>
{
public:
    stream_string_client_handler(const request_type& request, stub_ptr stub, grpc_client_queue_ptr queue);
    ~stream_string_client_handler() override;

private:
    void handle_call_state() override;
    void handle_write_state() override;
    void handle_wait_state() override;
    void handle_read_state() override;
    void handle_finish_state() override;
};

class stream_int_client_handler : public client_method_handler_otm<proto::NameRequest, proto::IntReply>
{
public:
    stream_int_client_handler(const request_type& request, stub_ptr stub, grpc_client_queue_ptr queue);
    ~stream_int_client_handler() override;

private:
    void handle_call_state() override;
    void handle_write_state() override;
    void handle_wait_state() override;
    void handle_read_state() override;
    void handle_finish_state() override;
};

// ---------------------------------------------------------------------------------------------------------------------

template <typename Handler>
class streams_container
{
public:
    streams_container(stub_ptr stub, grpc_client_queue_ptr queue) : _stub(stub), _queue(queue)
    {
        LOG("streams_container::ctor()");
    }

    void create(const std::string& name, int msec)
    {
        QTimer::singleShot(msec, [this, name]() {
            LOG("streams_container::create(name=" + name + ")");

            remove_old();

            // prepare request for new handler
            proto::NameRequest req;
            req.set_name(name);

            if (_handlers.count(name)) { // rewrite active handler
                _handlers[name]->cancel();
                _old_handlers.emplace(std::move(_handlers[name]));

                _handlers[name] = std::make_unique<Handler>(req, _stub, _queue);
            } else { // create new handler
                _handlers.emplace(name, std::make_unique<Handler>(req, _stub, _queue));
            }
        });
    }

private:
    void remove_old()
    {
        LOG("streams_container::remove_old()");

        for (auto it = _old_handlers.begin(); it != _old_handlers.end(); ++it) {
            if ((*it)->is_closed()) {
                LOG("streams_container::remove_old(): success");
                _old_handlers.erase(it);
                break;
            }
        }
    }

    stub_ptr _stub;
    grpc_client_queue_ptr _queue;

    std::unordered_map<std::string, std::unique_ptr<Handler>> _handlers;
    std::unordered_set<std::unique_ptr<Handler>> _old_handlers;
};

class client : public QObject
{
    Q_OBJECT

public:
    explicit client(uint16_t port);
    ~client() override;

    void send_ping(int msec = 1000);
    void create_stream_string(const std::string& name, int msec = 1000);
    void create_stream_int(const std::string& name, int msec = 1000);

private slots:
    void stop();
    void poll();

private:
    const uint16_t _port;
    QTimer _timer;

    stub_ptr _stub;
    grpc_client_queue_ptr _queue;

    streams_container<stream_string_client_handler> _string_container;
    streams_container<stream_int_client_handler> _int_container;
};

} // namespace frankenstein

