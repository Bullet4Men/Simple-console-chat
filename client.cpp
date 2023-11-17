#include <deque>
#include <iostream>
#include <thread>
#include <ctime>
#include <iomanip>
#include <boost/asio.hpp>
#include "message.h"

using boost::asio::ip::tcp;

typedef std::deque<message> message_queue;

class client
{
public:
    client(boost::asio::io_context& context, const tcp::resolver::results_type& endpoints)
            : context_(context), socket_(context)
    {
        do_connect(endpoints);
    }

    void write(const message& msg)
    {
        boost::asio::post(context_,[this, msg]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if (!write_in_progress)
                do_write();
        });
    }

    void close()
    {
        boost::asio::post(context_, [this]() { socket_.close(); });
    }

private:
    void do_connect(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(socket_, endpoints,
                                   [this](boost::system::error_code ec, const tcp::endpoint&){
            if (!ec) {
               do_read_header();
            }
        });
    }

    void do_read_header()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), message::header_length),
                                [this](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec && read_msg_.decode_header()) {
                do_read_body();
            } else {
                socket_.close();
            }
        });
    }

    void do_read_body()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                [this](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec) {
                std::cout.write(read_msg_.body(), read_msg_.body_length());
                std::cout << "\n";
                do_read_header();
            } else {
                socket_.close();
            }
        });
    }

    void do_write()
    {
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_msgs_.front().data(),
                                                     write_msgs_.front().length()),
                                 [this](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty())
                    do_write();
            } else {
                socket_.close();
            }
        });
    }

private:
    boost::asio::io_context& context_;
    tcp::socket socket_;
    message read_msg_;
    message_queue write_msgs_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: client <host> <port> <nickname>\n";
            return 1;
        }

        std::string nickname = argv[3];

        boost::asio::io_context context;
        boost::asio::thread_pool pool(std::thread::hardware_concurrency());

        tcp::resolver resolver(context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        client client(context, endpoints);

        boost::asio::post(pool, [&](){ context.run(); });

        char line[message::max_body_length + 1];
        while (std::cin.getline(line, message::max_body_length + 1)) {
            std::string input = line;

            time_t now = time(nullptr);
            tm* time_now = localtime(&now);

            std::stringstream ss;
            ss << "[" << nickname << "](" << std::put_time(time_now, "%H:%M") << "): " << input;

            message msg;
            msg.body_length(ss.str().length());
            std::memcpy(msg.body(), ss.str().c_str(), msg.body_length());
            msg.encode_header();
            client.write(msg);
        }

        client.close();
        pool.join();
    } catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
