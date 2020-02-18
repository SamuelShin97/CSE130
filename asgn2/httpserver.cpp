#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <locale>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sstream>
#include <pthread.h>
#include <vector>
#define PORT 80

using namespace std;

string HTTP200 = "HTTP/1.1 200 OK\r\n";
const char* HTTP200_response = HTTP200.c_str();

string HTTP201 = "HTTP/1.1 201 Created\r\n";
const char* HTTP201_response = HTTP201.c_str();

string HTTP400 = "HTTP/1.1 400 Bad Request\r\n";
const char* HTTP400_response = HTTP400.c_str();

string HTTP403 = "HTTP/1.1 403 Forbidden\r\n";
const char* HTTP403_response = HTTP403.c_str();

string HTTP404 = "HTTP/1.1 404 Not Found\r\n";
const char* HTTP404_response = HTTP404.c_str();

string HTTP500 = "HTTP/1.1 500 Internal Server Error\r\n";
const char* HTTP500_response = HTTP500.c_str();

//shared vars
vector<bool> available_threads;

bool check_for_warning(int indicator, char* filename, int sockfd)
{
   if (indicator == -1)
   {
      //figure out this warn to send errors to client
      warn("%s", filename);
      return true;
   }
   return false;
}

void log_fail(int log_fd, string header, int error_code)
{
   stringstream ss;
   ss << "FAIL: ";
   ss << header;
   ss << " ";
   ss << " --- response " + to_string(error_code) + "\n" + "========\n";
   string fail_msg = ss.str();
   //string fail_msg = "FAIL: " + header + " " + filename + " --- response " + to_string(error_code) + "\n" + "========\n";
   char array[fail_msg.length() + 1];
   strcpy(array, fail_msg.c_str());
   write(log_fd, array, sizeof(array));
}

void log_content_length(int log_fd, int bytes)
{
   string str = " length " + to_string(bytes) + "\n";
   char content_length[str.length() + 1];
   strcpy(content_length, str.c_str());
   write(log_fd, content_length, sizeof(content_length));
}

void log_data(int log_fd, char * buffer)
{
   uint32_t j = 0;
   string byte_count = "00000000"; 
   stringstream ss;
   ss << byte_count;
   ss << " ";
   for (uint32_t i = 0; i < strlen(buffer); i++)
   {
      if (j == 20)
      {
         j = 0;
	 uint32_t byte_cnt = stoi(byte_count);
	 byte_cnt += 20;
	 byte_count = to_string(byte_cnt);
	 for (int k = byte_count.length(); k < 8; k++)
	 {
	    byte_count.insert(0, "0");
	 }

	 ss << "\n";
	 ss << byte_count;
	 ss << " ";
      }
      ss << hex << (int)buffer[i];
      ss << " ";
      j++;
   }
   if (j != 1)
   {
      ss << "\n";
   }
   ss << "========\n";
   string data = ss.str();
   char data_array[data.length() + 1];
   strcpy(data_array, data.c_str());
   write(log_fd, data_array, sizeof(data_array));  
}

void get_file(char* file, int sockfd, bool log_requested, int log_fd)
{
   size_t cnt = 32000;
   char file_contents[cnt];
   int opened = open(file, O_RDONLY);
   cout << "opened " + to_string(opened) << endl;
   bool warning_given = check_for_warning(opened, file, sockfd);
   if (warning_given)
   {
      /*somehow decipher the warning and give either an error 403 or 404 depending on the 
       * warning given*/
      send(sockfd, HTTP404_response, sizeof(HTTP404), 0);
      exit(EXIT_FAILURE);
   }

   size_t bytes_read_sentinel= 1; //initialize to a non-zero value
   size_t total_bytes_read = 0;
   while (bytes_read_sentinel != 0)
   {
      size_t bytes_read = read(opened, &file_contents, cnt);
      total_bytes_read += bytes_read;
      if (!warning_given)
      {
         warning_given = check_for_warning(bytes_read, file, sockfd);
      }
      else
      {
         break;
      }

      send(sockfd, file_contents, bytes_read, 0);
      bytes_read_sentinel = bytes_read;

      if (bytes_read_sentinel == 0)
      {
         break;
      }
   }

   send(sockfd, HTTP200_response, sizeof(HTTP200), 0);
   //cout << "total number of bytes read " + to_string(total_bytes_read) << endl;
   string cont_len_resp = "Content-Length: " + to_string(total_bytes_read) + "\r\n";
   const char* content_length_response = cont_len_resp.c_str();
   send(sockfd, content_length_response, sizeof(cont_len_resp), 0); 

   if (log_requested)
   {
      //log_content_length(log_fd, total_bytes_read);
      string str = " length 0\n";
      char content_len[str.length() + 1];
      strcpy(content_len, str.c_str());
      write(log_fd, content_len, sizeof(content_len));
   }

   int closed = close(opened);
   if (!warning_given)
   {
      check_for_warning(closed, file, sockfd);
   }
   //cout << "end of get" << endl;

}

void put_file(char* file, char* buffer, size_t bytes, int sockfd, bool log_requested, int log_fd)
{
   chmod(file, S_IRWXU);
   int opened = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
   bool warning_given = check_for_warning(opened, file, sockfd);
   if (warning_given)
   {
      send(sockfd, HTTP403_response, sizeof(HTTP403), 0);
      if (log_requested)
      {
         //log_fail(log_fd, header, 403);
      }
   }
   write(opened, buffer, bytes);

   send(sockfd, HTTP201_response, sizeof(HTTP201), 0);

   if (log_requested)
   {
      log_content_length(log_fd, bytes);
      log_data(log_fd, buffer);   
   }

   int closed = close(opened);
   if (!warning_given)
   {
      check_for_warning(closed, file, sockfd);
   }
}

void give_error(string msg)
{
   cout << msg << endl;
   exit(EXIT_FAILURE);
}

void check_fd_success(int fd, string error_msg)
{
   if (fd < 0)
   {
      give_error(error_msg);
   }
}

string get_http_header(char* buffer)
{
   char single_char = buffer[0];
   string single_char_str = string(1, single_char);
   uint32_t index = 0;
   string header;
   while (single_char != '\r')
   {
       header.append(single_char_str);
       index++;
       single_char = buffer[index];
       single_char_str = string(1, single_char);
   }
   return header;
}

string get_content_length(char* buffer)
{
   char single_char = buffer[0];
   uint32_t index = 0;
   uint32_t line = 0;
   string content_length;
   while (true)
   {
      if (single_char == '\r')
      {
         line++;
      }
      if (line == 4)
      {
         string single_char_str = string(1, single_char);
         content_length.append(single_char_str);
      }
      if (line == 5)
      {
         break;
      }
      index++;
      single_char = buffer[index];
   }
   return content_length;
}

string header_long_enough(string header, int sockfd, bool log_requested, int log_fd)
{
   string fileheader = header.substr(4);
   if (fileheader.length() < 27)
   {
      send(sockfd, HTTP400_response, sizeof(HTTP400), 0);
      if (log_requested)
      {
	 log_fail(log_fd, header, 400);
      }
      exit(EXIT_FAILURE);
   }
   return fileheader;
}

string filename_is_27_char_long(string fileheader, int sockfd)
{
   string filename = fileheader.erase(27);
   size_t file_length = filename.length();
   if (file_length != 27)
   {
      send(sockfd, HTTP400_response, sizeof(HTTP400), 0);
      exit(EXIT_FAILURE);
   }
   return filename;
}

size_t get_content_length_bytes(string content_length)
{
   size_t pos = content_length.find(":");
   string bytes = content_length.substr(pos + 2);
   size_t numbytes = stoi(bytes);
   return numbytes;
}

void log_request(string header, int log_fd)
{
   string chopped_header = header.erase(31, 9);
   char header_array[chopped_header.length() + 1];
   strcpy(header_array, chopped_header.c_str());
   write(log_fd, header_array, sizeof(header_array));
}

struct Handle_Request_Params
{
   int placeholder;
   int thread_index;
   int sock_fd;
   int log_fd;
   bool log_requested;
};

void *handle_request(void * args)
{
   Handle_Request_Params *params = static_cast<Handle_Request_Params *>(args);
   int thread_index = params->thread_index;
   int new_sock = params->sock_fd;
   int log_fd = params->log_fd;
   bool log_requested = params->log_requested;
   size_t cnt = 32000;
   char buffer[cnt];
   //cout << "in handle_request" << endl;
   //cout << "on thread index " + to_string(thread_index) << endl; 
   //cout << "new_sock is " + to_string(new_sock) << endl;
   //cout << "log_fd is " + to_string(log_fd) << endl;
   //cout << "log_requested ";
   //cout << log_requested << endl;
   int recv_fd = recv(new_sock, &buffer, cnt, 0);
   check_fd_success(recv_fd, "ERROR: recv failed");
   string header = get_http_header(buffer);
   string http_signal = header.substr(0, 3);
   if (http_signal.compare("GET") == 0)
   {
      //cout << "in get" << endl;
      string fileheader = header_long_enough(header, new_sock, log_requested, log_fd);
      string filename = filename_is_27_char_long(fileheader, new_sock);
      size_t file_length = filename.length();
      char file_to_be_processed[file_length];
      for (uint32_t i = 0; i < file_length; i++)
      {
         if (!(isalnum(filename[i]) || filename[i] == '-' || filename[i] == '_'))
         {
            send(new_sock, HTTP400_response, sizeof(HTTP400), 0);
            exit(EXIT_FAILURE);
         }
         file_to_be_processed[i] = filename[i];
      }
      if (log_requested)
      {
         log_request(header, log_fd);
      }
      get_file(file_to_be_processed, new_sock, log_requested, log_fd);
   }

   else if (http_signal.compare("PUT") == 0)
   {
      string content_length = get_content_length(buffer);
      size_t numbytes = get_content_length_bytes(content_length);

      char data_buffer[numbytes];
      recv(new_sock, &data_buffer, numbytes, 0);

      string fileheader = header_long_enough(header, new_sock, log_requested, log_fd);
      string filename = filename_is_27_char_long(fileheader, new_sock);
      size_t file_length = filename.length();
      char file_to_be_processed[file_length];
      for (uint32_t i = 0; i < file_length; i++)
      {
         if (!(isalnum(filename[i]) || filename[i] == '-' || filename[i] == '_'))
         {
            send(new_sock, HTTP400_response, sizeof(HTTP400), 0);
            exit(EXIT_FAILURE);
         }
         file_to_be_processed[i] = filename[i];
      }
      if (log_requested)
      {
         log_request(header, log_fd);
      }
      put_file(file_to_be_processed, data_buffer, numbytes, new_sock, log_requested, log_fd);
   }
   else
   {
      send(new_sock, HTTP500_response, sizeof(HTTP500_response), 0);
   }
   available_threads[thread_index] = true;
   //cout << "end of handle_request" << endl;
   return 0;
}

int main(int argc, char** argv)
{
   int threads = 4;
   bool thread_num_specified = false;
   bool log_requested = false;
   string log_name;
   if (argc < 3 || argc > 6)
   {
      give_error("ERROR: incorrect amount of args given (use: ./httpserver [-Nx (x = number of threads(optional))] [-l [log file]] [address] [port number (optional)])");
   }

   int address_index = 2;
   char * address = argv[address_index];
   int option;
   while ((option = getopt(argc, argv, "N::l:")) != -1)
   {
      if (option == 'N')

      {
	 if (optarg != 0)
	 {
            threads = atoi(optarg);
	 }
         if (threads == 0)
         {
	    threads = 4;
	    
	 }
	 else
	 {
	    thread_num_specified = true;
	 }

      }
      else if (option == 'l')
      {
         log_requested = true;
         log_name = optarg;
	 address_index = 4;
         address = argv[address_index];
      }
      else if (option == ':')
      {
         give_error("argument not given after -N or -l");
      }
      else if (option == '?')
      {
	 give_error("invalid option given");
      }
   }
   
   bool port_specified = false;
   if (address_index + 1 <= argc)
   {
      port_specified = true;
   }

   pthread_t *worker_pool;
   worker_pool = (pthread_t *)malloc(sizeof(pthread_t) * threads);
   available_threads = vector<bool>(threads, true);
   //cout << "available_threads size " + to_string(available_threads.size()) << endl;
   
   char log[log_name.length() + 1];
   strcpy(log, log_name.c_str());
   int log_fd = open(log, O_CREAT | O_WRONLY | O_TRUNC, 0666);

   while(true)
   {
      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      check_fd_success(sockfd, "ERROR: socket failed");
   
      int opt = 1;
      int sockopt_success = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
      check_fd_success(sockopt_success, "ERROR: setsockopt failed");

      struct sockaddr_in addr;
      uint32_t addrlen = sizeof(addr);

      addr.sin_family = AF_INET;

      if (port_specified)
      {
         char *end;
         char * port_number = argv[argc - 1];
	 
         uint32_t port_num = strtoul(port_number, &end, 10);

         if (0 <= port_num && port_num <= 65535)
         {
            addr.sin_port = htons(port_num);
         }
         else
         {
            give_error("ERROR: invalid port number");
         }

      }
      else
      {
         addr.sin_port = htons(PORT);
      }

      if (inet_aton(address, &addr.sin_addr) == 0)
      {
         give_error("ERROR: invalid address");
      }


      int bind_success = bind(sockfd, (struct sockaddr *)&addr, addrlen);
      check_fd_success(bind_success, "ERROR: bind failed");

      int listen_success = listen(sockfd, 3);
      check_fd_success(listen_success, "ERROR: listen failed");
   
      int new_sock = accept(sockfd, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
      check_fd_success(new_sock, "ERROR: accept failed");
      if (new_sock >= 0)
      {
         //cout << "request accepted" << endl;
	 bool handled = false;
	 while (!handled)
	 {
            for (uint32_t i = 0; i < available_threads.size(); i++)
	    {
	       if (available_threads[i])
	       {
	          available_threads[i] = false;
	          handled = true;
	          //cout << "thread index " + to_string(i) << endl; 
	          //cout << "sock_fd is " + to_string(new_sock) << endl;
	          //cout << "log_fd is " + to_string(log_fd) << endl;
	          //cout << "log_requested is ";
	          //cout << log_requested << endl;  
	          Handle_Request_Params params;
	          params.placeholder = i;
	          params.thread_index = i;
	          params.sock_fd = new_sock;
	          params.log_fd = log_fd;
	          params.log_requested = log_requested;
	          Handle_Request_Params *params_ptr = &params;
	          pthread_create(&(worker_pool[i]), NULL, handle_request, (params_ptr));
                  break;
	       }
            }
	    if (handled)
	    {
	       //cout << "breaking out of while" << endl;
	       break;
            }
         }
      }

      /*size_t cnt = 32000;
      char buffer[cnt];
      recv(new_sock, &buffer, cnt, 0);

      string header = get_http_header(buffer);
      string http_signal = header.substr(0, 3);

      if (http_signal.compare("GET") == 0)
      {
         string fileheader = header_long_enough(header, new_sock);
         string filename = filename_is_27_char_long(fileheader, new_sock);
         size_t file_length = filename.length();
         char file_to_be_processed[file_length];
         for (uint32_t i = 0; i < file_length; i++)
         {
            if (!(isalnum(filename[i]) || filename[i] == '-' || filename[i] == '_'))
            {
               send(new_sock, HTTP400_response, sizeof(HTTP400), 0);
               exit(EXIT_FAILURE);
            }
            file_to_be_processed[i] = filename[i];
         }
         if (log_requested)
	 {
            log_request(header, log_fd);
	 }
         get_file(file_to_be_processed, new_sock, log_requested, log_fd);
      }
      else if (http_signal.compare("PUT") == 0)
      {
         string content_length = get_content_length(buffer);
         size_t numbytes = get_content_length_bytes(content_length);

         char data_buffer[numbytes];
         recv(new_sock, &data_buffer, numbytes, 0);

         string fileheader = header_long_enough(header, new_sock);
         string filename = filename_is_27_char_long(fileheader, new_sock);
         size_t file_length = filename.length();
         char file_to_be_processed[file_length];
         for (uint32_t i = 0; i < file_length; i++)
         {
            if (!(isalnum(filename[i]) || filename[i] == '-' || filename[i] == '_'))
            {
               send(new_sock, HTTP400_response, sizeof(HTTP400), 0);
               exit(EXIT_FAILURE);
            }
            file_to_be_processed[i] = filename[i];
         }
	 if (log_requested)
         {
            log_request(header, log_fd);
         }
         put_file(file_to_be_processed, data_buffer, numbytes, new_sock, log_requested, log_fd);
      }
      else
      {
         send(new_sock, HTTP500_response, sizeof(HTTP500_response), 0);
      }*/
   }
   return 0;
}

