#include <iostream>
#include <fstream>
#include <string>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

static inline bool file_exists(const std::string &name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

static constexpr char not_found[] =
    "HTTP/1.0 404 NOT FOUND\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<HTML><TITLE>Not Found</TITLE>\r\n"
    "<BODY><P>The server could not fulfill\r\n"
    "your request because the resource specified\r\n"
    "is unavailable or nonexistent.\r\n"
    "</BODY></HTML>\r\n";

static constexpr char not_implemeted[] =
    "HTTP/1.0 501 Method Not Implemented\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<HTML><HEAD><TITLE>Method Not Implemented\r\n"
    "</TITLE></HEAD>\r\n"
    "<BODY><P>HTTP request method not supported.\r\n"
    "</BODY></HTML>\r\n";

struct Resource {
    enum class type { FILE, STRING };
    type t;
    std::string res;
};

class Request {
public:
    enum class methods { GET, UNIMPLEMENTED };

    Request(std::string &s)
    {
        std::vector<std::string> splited;
        boost::algorithm::split(splited, s, boost::algorithm::is_space());
        if (splited[0] == "GET")
            method = methods::GET;
        else
            method = methods::UNIMPLEMENTED;
        uri = move(splited[1]);
        protocol = move(splited[2]);
    };

    Resource gen_response() const
    {
        if (method == methods::GET) {
            std::string file = uri == "/" ? "index.html" : uri.substr(1);
            std::cout << file << std::endl;
            if (!(file_exists(file))) {
                return {Resource::type::STRING, not_found};
            } else {
                return {Resource::type::FILE, file};
            }
        } else {
            return {Resource::type::STRING, not_implemeted};
        }
    }

private:
    methods method;
    std::string uri;
    std::string protocol;
};

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(boost::asio::io_context &io) : socket_(io)
    {
    }

    void start()
    {
        auto p = shared_from_this();
        boost::asio::async_read_until(
            socket_, boost::asio::dynamic_buffer(request_), "\r\n\r\n",
            [p, this](const boost::system::error_code &err, size_t len) {
                if (err) {
                    std::cout << "recv err:" << err.message() << "\n";
                    return;
                }
                std::string first_line =
                    request_.substr(0, request_.find("\r\n"));
                Request req(first_line);

                // process with request
                auto response = req.gen_response();
                if (response.t == Resource::type::FILE) {
                    std::string header = "HTTP/1.0 200 OK\r\n"
                                         "Content-Type: text/html\r\n\r\n";
                    std::ifstream fin(response.res);
                    // FIXME: Asynchronus read this file
                    std::string content((std::istreambuf_iterator<char>(fin)),
                                        std::istreambuf_iterator<char>());
                    boost::asio::async_write(
                        socket_, boost::asio::buffer(header + content),
                        [p, this](const boost::system::error_code &err,
                                  size_t len) { socket_.close(); });
                } else {
                    boost::asio::async_write(
                        socket_, boost::asio::buffer(response.res),
                        [p, this](const boost::system::error_code &err,
                                  size_t len) { socket_.close(); });
                }
            });
    }

    boost::asio::ip::tcp::socket &Socket()
    {
        return socket_;
    }

private:
    boost::asio::ip::tcp::socket socket_;
    std::string request_;
};

class HttpServer {
public:
    HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep)
        : io_(io), acceptor_(io, ep)
    {
    }

    void start()
    {
        auto p = std::make_shared<HttpConnection>(io_);
        acceptor_.async_accept(
            p->Socket(), [p, this](const boost::system::error_code &err) {
                if (err) {
                    std::cout << "accept err:" << err.message() << "\n";
                    return;
                }
                p->start();
                this->start();
            });
    }

private:
    boost::asio::io_context &io_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, const char *argv[])
{
    if (argc != 3) {
        std::cout << "usage: httpsvr ip port\n";
        return 0;
    }

    boost::asio::io_context io;
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(argv[1]),
                                      std::stoi(argv[2]));
    HttpServer hs(io, ep);
    hs.start();
    io.run();
    return 0;
}
