#include <csignal>

#include <frankenstein/commons.hpp>
#include <frankenstein/client.hpp>

int main(int argc, char** argv)
{
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    safe_application app(argc, argv);
    safe_application::setApplicationName("Frankenstein client");

    auto grpc_client = frankenstein::client(50051);

    // grpc_client.send_ping(500);
    // grpc_client.send_ping(1500);

    grpc_client.create_stream_string("user1", 500);
    grpc_client.create_stream_string("user1", 540);
    // grpc_client.create_stream_string("user1", 8000);

    grpc_client.create_stream_int("user1", 500);
    grpc_client.create_stream_int("user1", 540);
    // grpc_client.create_stream_int("user1", 8000);

    // grpc_client.create_stream_string("user2", 500);
    // grpc_client.create_stream_string("user2", 501);
    // grpc_client.create_stream_string("user2", 8000);

    // grpc_client.create_stream_int("user2", 500);
    // grpc_client.create_stream_int("user2", 501);
    // grpc_client.create_stream_int("user2", 8000);

    app.exec();

    return 0;
}
