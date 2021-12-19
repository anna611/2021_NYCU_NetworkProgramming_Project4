#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <array>
#include <string>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <memory>
#include <utility>
#include <fstream>

using namespace boost::asio;
using boost::asio::io_service;
using boost::asio::ip::tcp;
using namespace std;

io_service ioservice;
const string filePath_ = "./test_case/";

typedef struct{
	string host;
	string port;
	string file;
	int valid = 0;
}client_info;

client_info client_record[5];

class session
    :public std::enable_shared_from_this<session>
{
    public:
        session(ip::tcp::socket socket,const int id): tcp_socket(move(socket))
        {
                file_.open(filePath_+client_record[id].file, std::fstream::in);
                if(!file_.is_open()){
                    cerr << "no file:" << client_record[id].file <<endl;
                }
                index = id;
        }
        void start(){
            read_handler();
        }
        void encode(string& str) {
            string buffer;
            buffer.reserve(str.size());
            for(size_t i = 0; i != str.size(); ++i) {
                switch(str[i]) {
                    case '\'': buffer.append("&apos;");      break;
                    case '<':  buffer.append("&lt;");        break;
                    case '\"': buffer.append("&quot;");      break;
                    case '>':  buffer.append("&gt;");        break;
                    case '&':  buffer.append("&amp;");       break;
                    case '\n': buffer.append("&NewLine;");   break;
                    case '\r': buffer.append("");   break;
                    default:   buffer.append(&str[i], 1); break;
                }
            }
            str.swap(buffer);
        }

        void output_shell(int index ,string content){
            encode(content);
            string output = "<script>document.getElementById('s"+to_string(index)+"').innerHTML += '"+content+"';</script>";
            cout << output << flush;
        }
        void output_cmd(int index,string content){
            encode(content);
            string output = "<script>document.getElementById('s"+to_string(index)+"').innerHTML += '<b>"+content+"</b>';</script>";
            cout <<output <<flush;
        }

        bool do_write(int index){
            auto self(shared_from_this());
            string input;
            bool state = false;
            getline(file_,input);
            //cerr << "input:" <<input <<endl;
            if(input == "exit"){
                state = true;
            }
            input = input + "\n";
            //cerr << "before write" <<endl;
            boost::asio::async_write(tcp_socket, boost::asio::buffer(input.c_str(), input.size()),
                [this, self](boost::system::error_code ec, std::size_t ){

                });
            //cerr << "after write" <<endl;
            if(checkexit == false)
                output_cmd(index,input);

            return state;
        }

        void read_handler()
        {
            auto self(shared_from_this());
            memset(data_, 0, 4096);
            tcp_socket.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    string str(data_);
                   // cerr <<"command:" << str << endl;
                    output_shell(index,str);
                    //std::cout.write(bytes.data(), bytes_transferred);
                    if(boost::algorithm::contains(data_,"%")){
                        checkexit = do_write(index);
                        if(checkexit == true){
                            client_record[index].valid = 0;
                        }
                    }
                }
                else{
                    return;
                }
                read_handler();
            });
        }
        
    private:
        ifstream file_;
        bool checkexit = false;
        tcp::socket tcp_socket;
        enum { max_length = 4096 };
        char data_[max_length];
        int index;
};



void ParseQuery(string query){
    vector<string> spiltQuery;
    stringstream ss(query);
    string tok;
    int index;
    while (getline(ss, tok, '&')) {
        spiltQuery.push_back(tok);
    }   
    for(size_t i = 0;i < spiltQuery.size();++i){
        index = i /3;
        //cerr <<"index:" <<index << endl;
        stringstream tempss(spiltQuery[i]);
        while (getline(tempss, tok, '=')) {
        }
        if(i % 3 == 0){
            if(tok != "")
                client_record[index].host = tok;
            //cerr <<"host:" <<tok << endl;
        }
        else if(i % 3 == 1){
            if(tok != "")
                client_record[index].port = tok;
            //cerr <<"port:" <<tok << endl;
        }
        else if(i % 3 == 2){
            if(tok != "")
                client_record[index].file = tok;
            //cerr <<"file:" <<tok << endl;
        }
        if(client_record[index].host != "")
            client_record[index].valid = 1;
    }
    return;
}

void Printpanel(){
    string htmlContent = "";
    htmlContent+=
"<!DOCTYPE html>"
"<html lang=\"en\">"
"  <head>"
"    <meta charset=\"UTF-8\" />"
"    <title>NP Project 3 Console</title>"
"    <link"
"     rel=\"stylesheet\""
"     href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""
"     integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""
"      crossorigin=\"anonymous\""
"    />"
"   <link"
"      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""
"      rel=\"stylesheet\""
"    />"
"    <link"
"      rel=\"icon\""
"        type=\"image/png\""
"      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""
"    />"
"    <style>"
"      * {"
"        font-family: 'Source Code Pro', monospace;"
"        font-size: 1rem !important;"
"      }"
"      body {"
"        background-color: #212529;"
"      }"
"      pre {"
"        color: #cccccc;"
"      }"
"      b {"
"        color: #01b468;"
"      }"
"    </style>"
"  </head>"
"  <body>"
"    <table class=\"table table-dark table-bordered\">"
"      <thead>"
"        <tr> ";
        for(int i = 0;i < 5;i++){
            if(client_record[i].valid == 1){
                htmlContent += "<th scope=\"col\">"+client_record[i].host+":"+client_record[i].port+"</th>" ;
            }
        }
        htmlContent +=
"        </tr>"
"      </thead>"
"      <tbody>"
"        <tr>";
        for(int i = 0;i < 5;i++){
            if(client_record[i].valid == 1){
                htmlContent += "<td><pre id=\"s"+to_string(i)+"\" class=\"mb-0\"></pre></td>";
            }
        }
        htmlContent +=  
"        </tr>"
"      </tbody>"
"    </table>"
"  </body>"
"</html>";
    cout << htmlContent << flush;
}

class server{
    private:
    tcp::resolver resolv;
    ip::tcp::socket *tcp_socket[5];
    public:
        server()
            :resolv(ioservice){
                //cerr << "before query" <<endl;
                for(int i = 0;i < 5;++i){
                    if(client_record[i].valid == 1){
                        tcp::resolver::query q{client_record[i].host, client_record[i].port};
                        resolv.async_resolve(q, boost::bind( &server::resolve_handler, this,i,std::placeholders::_1,std::placeholders::_2));
                    }
                }
                //cerr << "after query" <<endl;
            }    
    private:
        void connect_handler(int index,const boost::system::error_code &ec)
        {
            if (!ec)
            {
                //cerr <<"connect_handelert "<<endl;
                make_shared<session>(move((*tcp_socket[index])), index)->start();
            }
        }

        void resolve_handler(int index,const boost::system::error_code &ec,
        tcp::resolver::iterator it)
        {
            if (!ec){
                //cerr <<"resolve_handeler"<<endl;
                tcp_socket[index] = new tcp::socket(ioservice);
                (*tcp_socket[index]).async_connect(*it,  boost::bind( &server::connect_handler, this,index,std::placeholders::_1));
            }
            else{
                cerr << "error:" <<ec <<endl;
            }
        }
        
};

int main()
{
    cout << "Content-type: text/html" << endl << endl;
    const char *tmp = getenv("QUERY_STRING");
    string env_var(tmp ? tmp : "");
    if (env_var.empty()) {
        cerr << "[ERROR] No such variable found!" << endl;
        exit(EXIT_FAILURE);
    }
    //cerr <<"Query:"<< env_var <<endl;
    ParseQuery(env_var);
    Printpanel();
    server s;
    ioservice.run();
}
