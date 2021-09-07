#include <frankenstein/client.hpp>

#include <chrono>

namespace frankenstein {

namespace chr = std::chrono;

client::client(uint16_t port) :
    _port(port),
    _timer(this),
    _stub(proto::ExchangeService::NewStub(
        ::grpc::CreateChannel("0.0.0.0:" + std::to_string(_port), ::grpc::InsecureChannelCredentials()))),
    _queue(std::make_shared<::grpc::CompletionQueue>()),
    _string_container(_stub, _queue),
    _int_container(_stub, _queue)
{
    LOG("client::ctor()");

    connect(&_timer, &QTimer::timeout, this, &client::poll);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &client::stop);

    _timer.start();
}

client::~client()
{
    LOG("client::dtor()");
    stop();
}

void client::stop()
{
    LOG("client::stop()");

    if (_queue)
        _queue->Shutdown();

    _timer.stop();
}

void client::poll()
{
    // LOG("client::poll()");

    void* tag;
    bool ok = false;

    // const chr::milliseconds wait_msec = 100; // NOTE: increased for testing
    const chr::milliseconds wait_msec{1}; // NOTE: increased for testing
    const auto deadline = chr::system_clock::now() + wait_msec;

    const auto status = _queue->AsyncNext(&tag, &ok, deadline);
    if (status == ::grpc::CompletionQueue::TIMEOUT || status == ::grpc::CompletionQueue::SHUTDOWN)
        return;

    auto* handler = static_cast<client_method_handler_stub*>(tag);
    handler->proceed(ok);
}

void client::send_ping(int msec)
{
    LOG("client::send_ping()");
    QTimer::singleShot(msec, [this]() {
        LOG("client::send_ping(): create handler");
        proto::EmptyRequest req;
        new ping_client_handler(req, _stub, _queue);
    });
}

void client::create_stream_string(const std::string& name, int msec)
{
    LOG("client::create_stream_string()");
    _string_container.create(name, msec);
}

void client::create_stream_int(const std::string& name, int msec)
{
    LOG("client::create_stream_int()");
    _int_container.create(name, msec);
}

// ---------------------------------------------------------------------------------------------------------------------

ping_client_handler::ping_client_handler(const request_type& request, stub_ptr stub, grpc_client_queue_ptr queue) :
    client_method_handler_oto<request_type, reply_type>()
{
    LOG("ping_client_handler::ctor()");
    _reader = stub->AsyncPing(&_context, request, queue.get());
    _reader->Finish(&_reply, &_status, reinterpret_cast<void*>(this));
}

ping_client_handler::~ping_client_handler()
{
    LOG("ping_client_handler::dtor()");
}

bool ping_client_handler::proceed(bool ok)
{
    LOG("ping_client_handler::proceed()");

    if (!ok) {
        LOG("ping_client_handler::proceed(): ok=false");
        return false;
    }

    if (_status.ok()) {
        LOG("ping_client_handler::proceed(): status success");
        LOG("result: " + _reply.msg());
    } else {
        LOG("ping_client_handler::proceed(): status fail");
    }

    delete this;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

stream_string_client_handler::stream_string_client_handler(const request_type& request,
                                                           stub_ptr stub,
                                                           grpc_client_queue_ptr queue) :
    client_method_handler_otm<proto::NameRequest, proto::StringReply>(request, stub, queue)
{
    LOG("stream_string_client_handler::ctor()");

    proceed(true);
}

stream_string_client_handler::~stream_string_client_handler()
{
    LOG("stream_string_client_handler::dtor()");
}

void stream_string_client_handler::handle_call_state()
{
    LOG("stream_string_client_handler::handle_call_state()");

    _reader = _stub->PrepareAsyncStreamString(&_context, _request, _queue.get());
    _state = handler_state::WRITE;
    _reader->StartCall(this);
}

void stream_string_client_handler::handle_write_state()
{
    LOG("stream_string_client_handler::handle_write_state()");

    _state = handler_state::WAIT;
    _reader->Read(&_reply, this);
}

void stream_string_client_handler::handle_wait_state()
{
    LOG("stream_string_client_handler::handle_wait_state()");

    // if (_reply validate) {
    _state = handler_state::READ;
    _reply.Clear();
    _reader->Read(&_reply, this);
    // }
    // else {
    //     state_ = handler_state::FINISH;
    //     _reader->Finish(&_status, this);
    // }
}

void stream_string_client_handler::handle_read_state()
{
    LOG("stream_string_client_handler::handle_read_state()");
    LOG("result: " + _reply.msg());

    if (_reply.msg() != "1")
        _state = handler_state::READ;
    else
        _state = handler_state::FINISH;

    _reply.Clear();
    _reader->Read(&_reply, this);
}

void stream_string_client_handler::handle_finish_state()
{
    LOG("stream_string_client_handler::handle_finish_state()");
    _state = handler_state::CLOSED;
}

// ---------------------------------------------------------------------------------------------------------------------

stream_int_client_handler::stream_int_client_handler(const request_type& request,
                                                     stub_ptr stub,
                                                     grpc_client_queue_ptr queue) :
    client_method_handler_otm<proto::NameRequest, proto::IntReply>(request, stub, queue)
{
    LOG("stream_int_client_handler::ctor()");

    proceed(true);
}

stream_int_client_handler::~stream_int_client_handler()
{
    LOG("stream_int_client_handler::dtor()");
}

void stream_int_client_handler::handle_call_state()
{
    LOG("stream_int_client_handler::handle_call_state()");

    _reader = _stub->PrepareAsyncStreamInt(&_context, _request, _queue.get());
    _state = handler_state::WRITE;
    _reader->StartCall(this);
}

void stream_int_client_handler::handle_write_state()
{
    LOG("stream_int_client_handler::handle_write_state()");

    _state = handler_state::WAIT;
    _reader->Read(&_reply, this);
}

void stream_int_client_handler::handle_wait_state()
{
    LOG("stream_int_client_handler::handle_wait_state()");

    // if (_reply validate) {
    _state = handler_state::READ;
    _reply.Clear();
    _reader->Read(&_reply, this);
    // }
    // else {
    //     state_ = handler_state::FINISH;
    //     _reader->Finish(&_status, this);
    // }
}

void stream_int_client_handler::handle_read_state()
{
    LOG("stream_int_client_handler::handle_read_state()");
    LOG("result: " + std::to_string(_reply.msg()));

    if (_reply.msg() != 1)
        _state = handler_state::READ;
    else
        _state = handler_state::FINISH;

    _reply.Clear();
    _reader->Read(&_reply, this);
}

void stream_int_client_handler::handle_finish_state()
{
    LOG("stream_int_client_handler::handle_finish_state()");
    _state = handler_state::CLOSED;
}

} // namespace frankenstein
