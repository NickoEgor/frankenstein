#include <csignal>

#include <frankenstein/commons.hpp>
#include <frankenstein/server.hpp>

int main(int argc, char** argv)
{
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    safe_application app(argc, argv);
    safe_application::setApplicationName("Frankenstein server");

    auto grpc_server = frankenstein::server(50051);
    grpc_server.init();

    app.exec();

    return 0;
}
