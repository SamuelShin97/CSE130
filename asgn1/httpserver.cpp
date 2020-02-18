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

bool check_for_warning(int indicator, char* filename)
{
   if (indicator == -1)
   {
      warn("%s", filename);
      return true;
   }
   return false;
}

void get_file(char* file, int sockfd)
{
   size_t cnt = 32000;
   char file_contents[cnt];

   int opened = open(file, O_RDONLY);
   bool warning_given = check_for_warning(opened, file);
   if (warning_given)
   {
      /*somehow decipher the warning and give either an error 403 or 404 depending on the 
       * warning given*/
   }

   size_t bytes_read_sentinel= 1; //initialize to a non-zero value
   size_t total_bytes_read = 0;
   while (bytes_read_sentinel != 0)
   {
      size_t bytes_read = read(opened, &file_contents, cnt);
      total_bytes_read += bytes_read;
      if (!warning_given)
      {
         warning_given = check_for_warning(bytes_read, file);
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

   string cont_len_resp = "Content-Length: " + to_string(total_bytes_read) + "\r\n";
   const char* content_length_response = cont_len_resp.c_str();
   send(sockfd, content_length_response, sizeof(cont_len_resp), 0); 

   int closed = close(opened);
   if (!warning_given)
   {
      check_for_warning(closed, file);
   }

}

void put_file(char* file, char* buffer, size_t bytes, int sockfd)
{
   chmod(file, S_IRWXU);
   int opened = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
   bool warning_given = check_for_warning(opened, file);
   if (warning_given)
   {
      /*if it's a private file give a error 403*/
   }
   write(opened, buffer, bytes);

   send(sockfd, HTTP201_response, sizeof(HTTP201), 0);

   int closed = close(opened);
   if (!warning_given)
   {
      check_for_warning(closed, file);
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

string header_long_enough(string header, int sockfd)
{
   string fileheader = header.substr(4);
   if (fileheader.length() < 27)
   {
      send(sockfd, HTTP400_response, sizeof(HTTP400), 0);
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

int main(int argc, char** argv)
{
   if (argc < 2 || argc > 3)
   {
      give_error("ERROR: incorrect amount of args given (use: ./httpserver [address] [port number (optional)])");
   }

   char * address = argv[1];
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   check_fd_success(sockfd, "ERROR: socket failed");
   
   int opt = 1;
   int sockopt_success = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
   check_fd_success(sockopt_success, "ERROR: setsockopt failed");

   struct sockaddr_in addr;
   uint32_t addrlen = sizeof(addr);

   addr.sin_family = AF_INET;

   if (argc == 3)
   {
      char *end;
      uint32_t port_num = strtoul(argv[2], &end, 10);

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
   
   size_t cnt = 32000;
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

      get_file(file_to_be_processed, new_sock);
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
      put_file(file_to_be_processed, data_buffer, numbytes, new_sock);
   }
   else
   {
      send(new_sock, HTTP500_response, sizeof(HTTP500_response), 0);
   }
   
   return 0;
}
