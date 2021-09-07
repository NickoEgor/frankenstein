#pragma once

#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>

#include <QtCore/QCoreApplication>

template <typename T>
inline static void LOG(T arg)
{
    std::cout << arg << std::endl;
}

class safe_application : public QCoreApplication
{
    using QCoreApplication::QCoreApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return QCoreApplication::notify(receiver, event);
        } catch (std::exception& ex) {
            LOG("Exception [" + std::string(receiver->metaObject()->className()) + "]: " + ex.what());
            QCoreApplication::instance()->quit();
        }

        return false;
    }
};

inline void signal_handler(int signal)
{
    LOG("Caught signal: " + std::to_string(signal));
    QCoreApplication::instance()->quit();
}

namespace frankenstein {

using grpc_status = ::grpc::Status;

} // namespace frankenstein
