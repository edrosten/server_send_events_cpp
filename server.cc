#include <iostream>
#include <cstring>
#include <cerrno>
#include <vector>
#include <iomanip>


#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>


using namespace std;

class VeryBasicServer
{
	private:
		int sockfd=-1; 
		int connection_fd{-1};
	
	public:

		VeryBasicServer()
		{
			//Incantation to create a socket 
			//Address famile: AF_INET (that is internet, aka IPv4)
			//Type is a reliable 2 way stream, aka TCP
			//Note this just creates a socket handle, it doesn't do anything yet.
			//The 0 is the socket type which is pretty redundant for SOCK_STREAM (only one option)
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd < 0)
				throw runtime_error("Error opening socket:"s + strerror(errno));

			
			//Set options at the socket level (a socket has a stack of levels, and 
			//all of them have options). This one allows reuse of the address, so that
			//if the program crashes, we don't have to wait for the OS to reclaim it, before
			//we can use this socket again. Useful for debugging!
			int true_ = 1; 
			setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&true_,sizeof(int));
			
			
			//Binding a socket is what sets the port number for a particular
			//socket.
			sockaddr_in serv_addr = {};
			serv_addr.sin_family = AF_INET;          //internet address family
			serv_addr.sin_addr.s_addr = INADDR_ANY;  //allow connections from any address
			serv_addr.sin_port = htons(6502);        //Still fighting the 80s CPU wars. 6502 > 8080
			if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
				throw runtime_error("Error binding socket:"s + strerror(errno));
			
			//This is necessary for stream sockets (e.g. TCP). When a client_ connects
			//a new socket will be created just for that connection on a high port.
			listen(sockfd,5);
		}
		

		void read_ignore_and_dump_incoming_data()
		{
			vector<char> buf(65536, 0);
			int n = read(connection_fd,buf.data(),buf.size());
			if (n < 0) 
				throw runtime_error("Error reading from socket:"s + strerror(errno));
			cout << "Message reads: ";
			cout.write(buf.data(), n);
		}

		void accept()
		{
			//Listen blocks until a cnnection is made, then hands over the newly created
			//socket for the connection
			connection_fd = ::accept(sockfd, nullptr, nullptr);

			if (connection_fd < 0) 
				throw runtime_error("Error accepting:"s + strerror(errno));
			
			//We can actually ignore the HTTP header just to get up
			//and running. For ServerSendEvents, there's only one accepted
			//MIME type, and uncompressed is always a valid compression option
			//even if the client_ doesn't request it.
			read_ignore_and_dump_incoming_data();


			//Construct a valid working header.
			//
			//Two important things to note. One is the access control. Since
			//this server isn't serving anything except SSEs, the web page
			//which is using them ust come from elsewhere. Unless we allow 
			//connections from places other than the originating server, then
			//the web browser will block the attempt for security.
			//
			//The other point is the chunked encodeing. The browser connecting
			//has to know when we've finished sending an event in the stream
			//of data. Chunked encoding allows us to send data blocks along with a 
			//length so the server knows when a block is finished. The other option
			//is to have a fixed Content-Length instead. I never got it working,
			//but it's much less flexible so I didn't try hard.
			//
			//Note also the \r\n line terminations, not just \n. Part of the HTTP spec.
			write("HTTP/1.1 200 OK\r\n"                          //Standard HTTP header line
			      "Content-Type: text/event-stream\r\n"          //This is the only allowed MIME type for SSE
                  "Transfer-Encoding: chunked\r\n"               //Chunked encoding lets it know when an event is done without knowing packet boundaries.
				  "Access-Control-Allow-Origin: *\r\n"           //Because the HTML comes from a file, not this server, we have to allow access
			      "\r\n");                                       //End of header indicator

		}
		
		//Write data with less than the minimal amount of error checking.
		void write(const string& str)
		{
			int n = ::write(connection_fd, str.data(), str.size());

			if(n < 0)
				throw runtime_error("Error writing:"s + strerror(errno));
		}

		void write_chunk(const string& str)
		{
			//Chunked encoding is the size in hex followed by the data. Note that both
			//the size and data fields are terminated by HTTP line terminations (\r\n)
			ostringstream o;
			o << hex << str.size() << "\r\n" << str << "\r\n";
			write(o.str());
		}

		~VeryBasicServer()
		{
			cerr << "closing\n";
			close(connection_fd);
			close(sockfd);
		}
};


int main()
{
	VeryBasicServer hax;

	hax.accept();
	
	cout << "Press enter to send an event" << endl;
	for(int i=1;;i++)
	{
		if(cin.get() == EOF)
			break;
		
		//Build up an event in server-send-event format. The message consists of 
		//one or more fields of the form:
		//field: value\n
		//Followed by an empty line.
		//
		//The main tag is "data:" which carries the data payload.
		//See https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events
		//for more info (e.g. different message types and dispatch)
		string sse;
		sse += "data: " + to_string(i) + "\n";  //Send the current number as the data value
		sse += "\n";                            //Empty field ends the message.

		//Send the message data using chunked encoding
		hax.write_chunk(sse);

	}

}
