#include <iostream>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <thread>
#include <vector>
#include <iomanip>


#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>


using namespace std;

class StupidFakeHttpServerLOL
{
	private:
		int sockfd=-1; 
		atomic<int> newsockfd{-1};
		int port=6502;
		socklen_t clilen = 0;
		sockaddr_in serv_addr = {};
		sockaddr_in cli_addr = {};
	
	public:

		StupidFakeHttpServerLOL()
		{
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd < 0)
				throw runtime_error("Error opening socket:"s + strerror(errno));

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = INADDR_ANY;
			serv_addr.sin_port = htons(port);

			int true_ = 1; 
			setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&true_,sizeof(int));

			if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
				throw runtime_error("Error binding socket:"s + strerror(errno));

			listen(sockfd,5);
		}
		

		void accept()
		{	
			clilen = sizeof(cli_addr);
			int fd = ::accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

			if (fd < 0) 
				throw runtime_error("Error accepting:"s + strerror(errno));

			vector<char> buf(65536, 0);

			int n = read(fd,buf.data(),buf.size());
			if (n < 0) 
				throw runtime_error("Error reading from socket:"s + strerror(errno));

			cout << "Message reads: ";
			cout.write(buf.data(), n);

			auto t = std::time(nullptr);
			ostringstream date;
			date << std::put_time(localtime(&t), "%a, %d %b %Y %H:%M:%S %Z");

			newsockfd = fd;
			write(R"(HTTP/1.1 200 OK
Date: )" + date.str() + R"(
Cache-Control: no-cache,public
Content-Type: text/event-stream
Connection: keep-alive
)");
			write("\n");

		}

		void write(const string& str)
		{
			if(newsockfd != -1)
			{
				int n = ::write(newsockfd, str.data(), str.size());

				if(n < 0)
				{
					cerr << "Error: " << strerror(errno);
					close(newsockfd);
					newsockfd=-1;
				}
			}
		}

		~StupidFakeHttpServerLOL()
		{
			cerr << "closing\n";
			close(newsockfd);
			close(sockfd);
		}
};


int main()
{
	StupidFakeHttpServerLOL hax;

	hax.accept();

	for(int i=0;;i++)
	{
		if(cin.get() == EOF)
			break;
		hax.write("data: wtf " + to_string(i) + "\n\n");

	}

}