#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <argp.h>
#include <sys/select.h>
#include <sys/time.h>
#include "../includes/hash.h"


struct server_arguments
{
   int port;
   char *salt;
   size_t salt_len;
};
error_t server_parser(int key, char *arg, struct argp_state *state)
{
   struct server_arguments *args = state->input;
   error_t ret = 0;
   switch (key)
   {
   case 'p':
      /* Validate that port is correct and a number, etc!! */
      args->port = atoi(arg);
      if (0 /* port is invalid */)
      {
         argp_error(state, "Invalid option for a port, must be a number");
      }
      break;
   case 's':
      args->salt_len = strlen(arg);
      args->salt = malloc(args->salt_len + 1);
      strcpy(args->salt, arg);
      break;
   default:
      ret = ARGP_ERR_UNKNOWN;
      break;
   }
   return ret;
}


int main(int argc, char *argv[])
{
   struct server_arguments args;

   /* bzero ensures that "default" parameters are all zeroed out */
   bzero(&args, sizeof(args));

   struct argp_option options[] = {
         {"port", 'p', "port", 0, "The port to be used for the server", 0},
         {"salt", 's', "salt", 0, "The salt to be used for the server. Zero by default", 0},
         {0}};
   struct argp argp_settings = {options, server_parser, 0, 0, 0, 0, 0};
   if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0)
   {
      printf("Got an error condition when parsing\n");
      return EXIT_FAILURE;
   }

   /* What happens if you don't pass in parameters? Check args values
* for sanity and required parameters being filled in */
   /* If they don't pass in all required settings, you should detect
* this and return a non-zero value from main */
   printf("Got port %d and salt %s with length %ld\n", args.port, args.salt, args.salt_len);

   if (args.port <= 1024)
   {
      printf("invalid parameters supplied");
      return EXIT_FAILURE;
   }
   in_port_t serv_port = args.port;
   int server_sock; // Socket  for server
   if ((server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      printf("socket() creation failed");
      return EXIT_FAILURE;
   }

   struct sockaddr_in servAddr;
   memset(&servAddr, 0, sizeof(servAddr));
   servAddr.sin_family = AF_INET;
   servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servAddr.sin_port = htons(serv_port);

   if (bind(server_sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
   {
      printf("bind() failed");
      return EXIT_FAILURE;
   }
   fputs("Listening..\n", stdout);
   if (listen(server_sock, 3) < 0)
   {
      printf("listen() failed");
      return EXIT_FAILURE;
   }
   int opt = 1;
   if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
   {
      perror("setsockopt error");
      return EXIT_FAILURE;
   }

   //preparing for multi client functionality
   fd_set readfds, copy;
   int activity;
   FD_ZERO(&readfds); //clear the socket set
   FD_SET(server_sock, &readfds);
   while (1){
      
      copy = readfds;
      activity = select(FD_SETSIZE, &copy, NULL, NULL, NULL);
      if (activity < 0){
         perror("select error");
         return EXIT_FAILURE;
      }
      struct sockaddr_in clientAddr;
      socklen_t clntAddrLen = sizeof(clientAddr);
      int client_sock;

      for (int i = 0; i < FD_SETSIZE; i++){
         if (FD_ISSET(i, &copy)){
            if (i == server_sock){ //this is a new connection
               // connecting to the new client
               client_sock = accept(server_sock, (struct sockaddr *)&clientAddr, &clntAddrLen);
               if (client_sock < 0){
                  printf("accept() failed");
                  return EXIT_FAILURE;
               }
               FD_SET(client_sock, &readfds);
               
            }else{ // handles client
               client_sock = i;
               char clntName[INET_ADDRSTRLEN]; // String to contain client address
               if (inet_ntop(AF_INET, &clientAddr.sin_addr.s_addr, clntName, sizeof(clntName)) != NULL){
                  printf("Handling client %s/%d\n", clntName, ntohs(clientAddr.sin_port));
               }else{
                  puts("Unable to get client address");
                  continue;
               }

               //recieving initialization msg
               uint16_t rcvdType;
               u_int32_t rcvdN;
               ssize_t bytesRcvd = recv(client_sock, &rcvdType, 2, 0);
               bytesRcvd += recv(client_sock, &rcvdN, 4, 0);
               if (bytesRcvd != 6)
               {
                  printf("error: recieving init failed\n");
                  return EXIT_FAILURE;
               }
               //printf("debug4\n");
               int N = ntohl(rcvdN);

               //create acknowledgement msg and send
               uint16_t ack_type = htons(2);
               uint32_t len_total_responses = 38;
               len_total_responses = len_total_responses * N;
               len_total_responses = htonl(len_total_responses);
               //printf("38*n: %08x\n", len_total_responses);

               ssize_t num_bytes_type = send(client_sock, &ack_type, 2, 0);
               ssize_t num_bytes_len = send(client_sock, &len_total_responses, 4, 0);
               if (num_bytes_type + num_bytes_len != 6)
               {
                  printf("error: sending ack failed\n");
                  return EXIT_FAILURE;
               }
               //printf("debug5\n");
               //printf("N: %d\n", N);

               // recieving all N hashrequests
               uint16_t type_num = 0;
               uint32_t data_size;
               uint8_t *payload;
               bytesRcvd = recv(client_sock, &type_num, 2, 0);
               //printf("type3_num: %d\n", ntohs(type_num));

               struct checksum_ctx *my_hash;
               uint32_t nCount = 0;
               while (bytesRcvd > 0)
               { // 0 indicates end of hashrequests being recieved

                  //printf("type number: %04x\n", type_num);
                  bytesRcvd += recv(client_sock, &data_size, 4, 0);
                  //printf("bytes recieved: %ld\n", bytesRcvd);
                  data_size = ntohl(data_size);
                  
                  payload = malloc(data_size);
                  payload[data_size] = '\0';
                  bytesRcvd += recv(client_sock, payload, data_size, 0);
                  // printf("data size: %d\n", data_size);
                  //printf("data: %s\n", payload);
                  //printf("total bytes recieved: %ld\n", bytesRcvd);

                  // preparing and sending hashresponse
                  if (args.salt_len == 0)
                     my_hash = checksum_create(NULL, 0);
                  else
                     my_hash = checksum_create((unsigned char *)args.salt, args.salt_len);
                  if (my_hash == NULL){
                     perror("check_create error\n");
                     return EXIT_FAILURE;
                  }
                  uint8_t out[32];
                  memset(out, 0, sizeof(out));

                  if (data_size <= 4096){
                     
                     int finish_check = checksum_finish(my_hash, payload, data_size, out);
                     if (finish_check){
                        perror("checksum_finish error\n");
                        return EXIT_FAILURE;
                     }
                  }
                  else{
                     FILE *stream = fmemopen(payload, data_size, "r");
                     ssize_t update_bytes = 0;

                     char update_buf[4096 + 1];
                     strcpy(update_buf, "");
                     while (update_bytes + 4096 < data_size && update_bytes + 4096 < 1000000){
                        fread(update_buf, 4096, 1, stream);
                        update_buf[4096] = '\0';
                        //printf("update buf: %s\n", update_buf);
                        int update_success = checksum_update(my_hash, (unsigned char *)update_buf);
                        if (update_success != 0)
                           return EXIT_FAILURE;
                        update_bytes += 4096;
                        strcpy(update_buf, "");
                     }
                     if(update_bytes + 4096 >= 1000000)
                        update_bytes = 1000000 - update_bytes;
                     else
                        update_bytes = data_size - update_bytes;
                     fread(update_buf, update_bytes, 1, stream);
                     int finish_check = checksum_finish(my_hash, (unsigned char *)update_buf, update_bytes, out);
                     if (finish_check){
                        perror("checksum_finish error\n");
                        return EXIT_FAILURE;
                     }
                     fclose(stream);
                  }

                  //printf("HASH:\n");
                  //for (unsigned int j = 0; j < sizeof(out); j++)
                     //printf("%02x", (unsigned int)out[j]);
                  //printf("\n\n");

                  //preparing and sending hashresponses here
                  uint16_t type4_num = htons(4);
                  ssize_t bytes_sent = 0;
                  nCount = htonl(nCount);
                  bytes_sent += send(client_sock, &type4_num, 2, 0);
                  bytes_sent += send(client_sock, &nCount, 4, 0);
                  bytes_sent += send(client_sock, out, 32, 0);
                  nCount = ntohl(nCount);
                  nCount++;

                  // getting type 3 and updating bytes recieved
                  bytesRcvd = recv(client_sock, &type_num, 2, 0);
                  if (bytesRcvd < 0)
                     return EXIT_FAILURE;
                  checksum_reset(my_hash);
                  free(payload);
                  payload = NULL;
               }
               checksum_destroy(my_hash);
               FD_CLR(i, &readfds);
            }
         }
      }
   }
   close(server_sock);
   free(args.salt);
   return 0;
}
