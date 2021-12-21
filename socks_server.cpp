//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <sstream>

using boost::asio::io_service;
using boost::asio::ip::tcp;
using namespace std;

io_service io_context;



class session
  : public std::enable_shared_from_this<session>
{
public:
  tcp::socket socket_;
  enum { max_length = 1024 };
  unsigned char message[max_length] = {0};
  char clientdata[200] ;
  char hostdata[200] ;
  session(tcp::socket socket)
    : socket_(std::move(socket)),resolv(io_context),bindAcceptor(io_context)
  {
    memset(clientdata,0,200);
    memset(hostdata,0,200);
  }

  void start()
  {
    do_read();
  }

private:
  tcp::resolver resolv;
  tcp::socket *connectsocket;
  tcp::acceptor bindAcceptor;
  unsigned char bindreply[8] = {0};
  void do_read()
  {
    auto self(shared_from_this());
    //cerr <<"before callback" <<endl;
    socket_.async_read_some(boost::asio::buffer(message, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            //cerr <<"after callback" <<endl;
            unsigned char VN = message[0];
            unsigned char CD = message[1];
            int ip[4] = {0};
            int port;
            bool check = false;
            bool socks4a = false;
            string domainname = "";
            string sock4aip = "";
            port = (int)message[2]*256+(int)message[3];
            for(int i = 0; i < 4;++i)
              ip[i] = (int)message[i+4];
            socks4a = checksocks4a(ip);
            if(socks4a == true){
                for(int i = 8;i < (int)sizeof(message);++i){
                  if(message[i] != 0){
                    domainname += message[i];
                  }
                }
              tcp::resolver::query q{domainname, to_string(port)};
              tcp::resolver::results_type results= resolv.resolve(q);
              tcp::endpoint endPoint = *results;
              boost::asio::ip::address tempsocks4a = endPoint.address();
              sock4aip = tempsocks4a.to_string();
            }
            check = checkfirewall(CD,ip);
            if(VN != 4){
              cerr << "Not socks4 request" << endl;
              check = false;
            }
            showMessage(CD,ip,port,check,sock4aip);
            if(check == true){
                if(CD == 1)
                    connectOperation(ip,port,domainname);
                else if(CD == 2)
                    bindOperation();
            }
            else{
                rejectMessage(message);
                socket_.close();
                exit(0);
            }
            //do_write(length);
          }
          else{
            //cerr << ec << endl;
          }
        });
  }
  bool checksocks4a(int ip[]){
      if((ip[0] == 0 ) && (ip[1] == 0 )&&(ip[2] == 0 )&&(ip[3] == 1 ))
      {
        return true;
      }
      else{
        return false;
      }
  }
  void bindOperation(){
      auto self(shared_from_this());
      unsigned short bindPort;
      connectsocket = new tcp::socket(io_context);
      tcp::endpoint endPoint(tcp::v4(),0);
      bindAcceptor.open(endPoint.protocol());
      bindAcceptor.set_option(tcp::acceptor::reuse_address(true));
      bindAcceptor.bind(endPoint);
      bindAcceptor.listen();
      bindPort = bindAcceptor.local_endpoint().port();
      bindreply[0] = 0;
      bindreply[1] = 90;
      bindreply[2] = (unsigned char)(bindPort/256);
      bindreply[3] = (unsigned char)(bindPort%256);
      for(int i = 4; i < 8;i++){
          bindreply[i] = 0;
      }
      socket_.async_send(boost::asio::buffer(bindreply, 8),
      [this,self](boost::system::error_code ec, std::size_t len){
      if(!ec){
          //cerr << "bind send call back" <<endl;
          auto self(shared_from_this());
          bindAcceptor.async_accept((*connectsocket),
          [this,self](boost::system::error_code err){
              if(!err){
                socket_.async_send(boost::asio::buffer(bindreply, 8),
                  [this,self](boost::system::error_code err2, std::size_t len){
                      if(!err2){
                          readClient();
                          readHost();
                      }
                      else{
                          //cerr << "bind send error:" << err2.message() <<endl;
                      }
                  });
              }
              else{
                  //cerr << "bind accept error:" << err.message() <<endl;
              }

          });
      }
      });
  }

  void writeHost(std::size_t length){
      auto self(shared_from_this());
      (*connectsocket).async_send(boost::asio::buffer(clientdata, length),
      [this,self](boost::system::error_code ec, std::size_t len){
          if(!ec){
              memset(clientdata,0,200);
              readClient();
          }
          else{
            //cerr << "send error:"<< ec.message() << endl;
          }
      });
  }
  void writeClient(std::size_t length){
      auto self(shared_from_this());
      socket_.async_send(boost::asio::buffer(hostdata, length),
      [this,self](boost::system::error_code ec, std::size_t len){
          if(!ec){
              memset(hostdata,0,200);
              readHost();
          }
          else{
            //cerr << "send error:"<< ec.message() << endl;
          }
      });
  }
  void readClient(){
      auto self(shared_from_this());
      socket_.async_read_some(boost::asio::buffer(clientdata, 200),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                //cerr << "clientdata"<<clientdata <<endl;
                writeHost(length);
            }
            else if(ec == boost::asio::error::eof){
                (*connectsocket).async_send(boost::asio::buffer(clientdata, length),
                [this,self](boost::system::error_code ec, std::size_t len){
                    if(!ec){

                    }
                    else{
                        //cerr << "send error:"<< ec.message() << endl;
                    }
                });
                connectsocket->shutdown(boost::asio::ip::tcp::socket::shutdown_send);
            }
            else{
                //cerr << "read client data error:" << ec.message() << endl;
            }
        });
  }
  void readHost(){
      auto self(shared_from_this());
      (*connectsocket).async_read_some(boost::asio::buffer(hostdata, 200),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                //cerr << "hostdata"<<hostdata <<endl;
                writeClient(length);
            }
            else if(ec == boost::asio::error::eof){
                socket_.async_send(boost::asio::buffer(hostdata, length),
                [this,self](boost::system::error_code ec, std::size_t len){
                    if(!ec){

                    }
                    else{
                        //cerr << "send error:"<< ec.message() << endl;
                    }
                });
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
            }
            else{
                //cerr << "read host data error"<< ec.message() << endl;
            }
        });
  }
  void connect_handler(const boost::system::error_code &ec)
  {
      auto self(shared_from_this());
      if (!ec)
      {
          //cerr << "connect handler" <<endl;
          //reply to socks client
          unsigned char reply[8] = {0};
          reply[0] = 0;
          reply[1] = 90;
          for(int i = 2; i < 8;i++){
            reply[i] = 0;
          }
          socket_.async_send(boost::asio::buffer(reply, 8),
          [this,self](boost::system::error_code ec, std::size_t len){
          if(!ec){
              //cerr << "connect call back" <<endl;
              readClient();
              readHost();
          }
          });
      }
  }
  void resolve_handler(const boost::system::error_code &ec,
        tcp::resolver::iterator it){
      auto self(shared_from_this());
      //cerr << "resole handler" <<endl;
      if (!ec){
          //cerr <<"before connect"<<endl;
          connectsocket = new tcp::socket(io_context);
          (*connectsocket).async_connect(*it,  boost::bind( &session::connect_handler, self,std::placeholders::_1));
          //cerr <<"after connect"<<endl;
      }
      else{
          //cerr << "error:" <<ec.message() <<endl;
      }
  }
  void connectOperation(int ip[],int port,string domainname){
      auto self(shared_from_this());
      //cerr << "connectOperation" <<endl;
      string host = "";
      if(domainname != ""){
        host = domainname;
      }
      else{
        for(int i = 0;i < 4;++i){
            host += to_string(ip[i]);
            if(i != 3)
              host += ".";
        }
      }
      //cerr << "host:" << host << endl;
      //cerr << "port:" << port << endl;
      tcp::resolver::query q{host, to_string(port)};
      resolv.async_resolve(q, boost::bind( &session::resolve_handler, self,std::placeholders::_1,std::placeholders::_2));
  }
  
  bool checkfirewall(unsigned char CD,int ip[]){
      //cerr <<"infirewall" <<endl;
      fstream file_;
      file_.open("./socks.conf",std::fstream::in);
      string grant, mode, iprule;
      int addr[4] = {0};
      string deli = ".";
      size_t pos = 0;
      string token;
      bool permit = true;
      //check socks.conf contains rules
      if(file_.is_open()) {
        while(!file_.eof()) {
            file_ >> grant >> mode >> iprule ;
            for(int i = 0;i < 4; ++ i){
              stringstream ss;
              if((pos =iprule.find(deli)) != std::string::npos){
                  token = iprule.substr(0, pos);
                  ss.str(token);
                  ss >> addr[i];
                  iprule.erase(0, pos + deli.length());
              }
              if(i == 3){
                  ss << iprule;
                  ss >> addr[i];
              }
            }
            if((CD == 1 && mode == "c") || (CD == 2 && mode == "b")){
                for(int i = 0;i < 4;i++){
                  if(addr[i] != 0 && addr[i] != ip[i]){
                      permit = false;
                      break;
                  }
                }
            }

        } 
      }
      else {
        permit = true;
        //cerr << "File open error" << endl;
      }
      file_.close();
      return permit;
  }
  void showMessage(unsigned char CD,int ip[],int port,bool check,string sock4aip){
      cout << "<S_IP>: " << socket_.remote_endpoint().address() << endl;
      cout << "<S_PORT>: " << socket_.remote_endpoint().port() << endl;
      if(sock4aip != ""){
          cout << "<D_IP>: " << sock4aip << endl;
      }
      else{
          cout << "<D_IP>: " << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << endl;
      }
      cout << "<D_PORT>: " << port << endl;
      if(CD == 1){
        cout << "<Command>: CONNECT" << endl;
      }
      else if(CD == 2){
        cout << "<Command>: BIND" << endl;
      }
      if(check == true){
        cout << "<Reply>: Accept" << endl;
      }
      else{
        cout << "<Reply>: Reject" << endl;
      }
      return;
  }
  void rejectMessage(unsigned char *msg){
      auto self(shared_from_this());
      unsigned char reply[8] = {0};
      reply[0] = 0;
      reply[1] = 91;
      for(int i = 2; i < 8;i++){
        reply[i] = msg[i];
      }
      socket_.async_send(boost::asio::buffer(reply, 8),
      [self](boost::system::error_code ec, std::size_t len){
          if(!ec){

          }
      });
      return;
  }
  
};

class server
{
public:
  server(short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            //std::make_shared<session>(std::move(socket))->start();
            io_context.notify_fork(io_service::fork_prepare);
            if (fork() > 0) {
              io_context.notify_fork(io_service::fork_parent);
              socket.close();
            } else {
              io_context.notify_fork(io_service::fork_child);
              acceptor_.close();
              std::make_shared<session>(std::move(socket))->start();
            }
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    server s(std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    //std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}