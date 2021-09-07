#include <frankenstein/server.hpp>

#include <chrono>
#include <thread>

namespace frankenstein {

namespace chr = std::chrono;
using namespace std::chrono_literals;

server::server(uint16_t port) : _port(port), _timer(this)
{
    LOG("server::ctor()");

    connect(&_timer, &QTimer::timeout, this, &server::poll);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &server::stop);
}

server::~server()
{
    LOG("server::dtor()");
    stop();
}

void server::init()
{
    LOG("server::init()");

    const std::string server_address("0.0.0.0:" + std::to_string(_port));

    _service = std::make_shared<async_service>();
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());
    builder.RegisterService(_service.get());
    builder.SetSyncServerOption(::grpc::ServerBuilder::MAX_POLLERS, 1);
    _queue = grpc_server_queue_ptr(builder.AddCompletionQueue());
    _server = grpc_server_ptr(builder.BuildAndStart());

    // handlers
    // NOTE: CompletionQueue segfaults when no handlers
    new ping_server_handler(_service, _queue);
    new stream_string_server_handler(_service, _queue);
    new stream_int_server_handler(_service, _queue);

    _timer.start();
}

void server::stop()
{
    LOG("server::stop()");

    _timer.stop();

    if (_server)
        _server->Shutdown();

    if (_queue)
        _queue->Shutdown();
}

void server::poll()
{
    // LOG("server::poll()");

    void* tag;
    bool ok = false;

    // const chr::milliseconds wait_msec{100}; // NOTE: increased for testing
    const chr::milliseconds wait_msec{1}; // NOTE: increased for testing
    const auto deadline = chr::system_clock::now() + wait_msec;

    const auto status = _queue->AsyncNext(&tag, &ok, deadline);
    if (status == ::grpc::CompletionQueue::TIMEOUT || status == ::grpc::CompletionQueue::SHUTDOWN)
        return;

    static_cast<server_method_handler_stub*>(tag)->proceed(ok);
}

// ---------------------------------------------------------------------------------------------------------------------

ping_server_handler::ping_server_handler(async_service_ptr service, grpc_server_queue_ptr queue) :
    server_method_handler_oto<async_service_ptr, proto::EmptyRequest, proto::StringReply>(service, queue)
{
    LOG("ping_server_handler::ctor()");
    _state = handler_state::PROCESS;
    _service->RequestPing(&_context, &_request, &_responder, _queue.get(), _queue.get(), this);
}

bool ping_server_handler::proceed(bool ok)
{
    LOG("ping_server_handler::proceed()");

    if (!ok) {
        LOG("ping_server_handler::proceed(): ok=false");
        return false;
    }

    switch (_state) {
        case handler_state::PROCESS: {
            LOG("ping_server_handler::proceed(): status=process");
            new ping_server_handler(_service, _queue);
            _reply.set_msg("pong");
            _state = handler_state::FINISH;
            _responder.Finish(_reply, grpc_status::OK, this);
            break;
        }
        default:
            LOG("ping_server_handler::proceed(): status=finish");
            GPR_ASSERT(_state == handler_state::FINISH);
            delete this;
            break;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

stream_string_server_handler::stream_string_server_handler(async_service_ptr service, grpc_server_queue_ptr queue) :
    server_method_handler_otm<async_service_ptr, proto::NameRequest, proto::StringReply>(service, queue)
{
    LOG("stream_string_server_handler::ctor()");
    proceed(true);
}

stream_string_server_handler::~stream_string_server_handler()
{
    LOG("stream_string_server_handler::dtor()");
}

void stream_string_server_handler::handle_call_state()
{
    LOG("stream_string_server_handler::handle_call_state()");

    _state = handler_state::WAIT;
    _service->RequestStreamString(&_context, &_request, &_responder, _queue.get(), _queue.get(), this);
}

void stream_string_server_handler::handle_wait_state()
{
    LOG("stream_string_server_handler::handle_wait_state()");

    new stream_string_server_handler(_service, _queue);

    _state = handler_state::WRITE;
    _responder.Write(_reply, this);

    // NOTE: if first reply state invalid
    /* _state = handler_state::FINISH; */
    /* _responder.WriteAndFinish(_reply, ::grpc::WriteOptions(), grpc_status::OK, this); */
}

void stream_string_server_handler::handle_write_state()
{
    LOG("stream_string_server_handler::handle_write_state()");

    if (amount > 0) {
        // std::this_thread::sleep_for(1000ms);
        std::this_thread::sleep_for(10ms);
        _reply.set_msg(std::to_string(amount--));
        LOG("stream_string_server_handler::handle_write_state(): write '" + _reply.msg() + "'");
        _responder.Write(_reply, this);
    } else {
        _state = handler_state::FINISH;
        _responder.WriteAndFinish(_reply, ::grpc::WriteOptions(), grpc_status::OK, this);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

stream_int_server_handler::stream_int_server_handler(async_service_ptr service, grpc_server_queue_ptr queue) :
    server_method_handler_otm<async_service_ptr, proto::NameRequest, proto::IntReply>(service, queue)
{
    LOG("stream_int_server_handler::ctor()");
    proceed(true);
}

stream_int_server_handler::~stream_int_server_handler()
{
    LOG("stream_int_server_handler::dtor()");
}

void stream_int_server_handler::handle_call_state()
{
    LOG("stream_int_server_handler::handle_call_state()");

    _state = handler_state::WAIT;
    _service->RequestStreamInt(&_context, &_request, &_responder, _queue.get(), _queue.get(), this);
}

void stream_int_server_handler::handle_wait_state()
{
    LOG("stream_int_server_handler::handle_wait_state()");

    new stream_int_server_handler(_service, _queue);

    _state = handler_state::WRITE;
    _responder.Write(_reply, this);

    // NOTE: if first reply state invalid
    /* _state = handler_state::FINISH; */
    /* _responder.WriteAndFinish(_reply, ::grpc::WriteOptions(), grpc_status::OK, this); */
}

void stream_int_server_handler::handle_write_state()
{
    LOG("stream_int_server_handler::handle_write_state()");

    if (amount > 0) {
        // std::this_thread::sleep_for(1000ms);
        std::this_thread::sleep_for(10ms);
        _reply.set_msg(amount--);
        LOG("stream_int_server_handler::handle_write_state(): write '" + std::to_string(_reply.msg()) + "'");
        _responder.Write(_reply, this);
    } else {
        _state = handler_state::FINISH;
        _responder.WriteAndFinish(_reply, ::grpc::WriteOptions(), grpc_status::OK, this);
    }
}

} // namespace frankenstein
