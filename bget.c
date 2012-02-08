/*
 * Copyright (C) 2012, Louis Liu <louisliu_at_locomotion_dot_tw>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_SPLIT 26
static char *File_Suffix = "abcdefghijklmnopqristuvwxyz";
static char *Interfaces[MAX_SPLIT] = {NULL};
static char Output_File_Name[PATH_MAX + 1] = "";
static char *Url;
static long long Content_Len = 0;
static long long Content_Len_Suggest = 0;
static int If_Count = 0;

static void usage(const char *name, int status);
static int curl_operation(void);
int main(int argc, char *argv[])
{
   int opt;
   while((opt = getopt(argc, argv, "o:i:hs:S:")) != -1){
      switch(opt){
      case 'o':
         strcpy(Output_File_Name, optarg);
         break;
      case 'i':
         /* other interfaces are ignored. */
         if( If_Count < MAX_SPLIT ){
            Interfaces[If_Count] = strdup(optarg);
            If_Count++;
         }
         break;
      case 'h':
         usage(argv[0], EXIT_SUCCESS);
         break;
      case 's':
         Content_Len_Suggest = atoll(optarg);
         break;
      case 'S':
         Content_Len = atoll(optarg);
         break;
      default:
         usage(argv[0], EXIT_FAILURE);
         break;
      }
   }

   if( optind >= argc ){
      usage(argv[0], EXIT_FAILURE);
   }
   if(Output_File_Name[0] == '\0'){
      usage(argv[0], EXIT_FAILURE);
   }
   Url = strdup(argv[optind]);

   printf("If_Count = %d\n", If_Count);
   if( curl_operation() < 0)
      exit(EXIT_FAILURE);
   else
      exit(EXIT_SUCCESS);
}

static void usage(const char *name, int status)
{
   fprintf(stderr, "%s [-i if_1] [-i if_2] -o file URL\n", name);
   exit(status);
}

struct user_handle_info {
   CURL *handle;
   FILE *file_fp;
};


/* only curl_operation uses these two functions */
static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
static int curl_multi_operation(CURLM *handle, struct user_handle_info *);
static int curl_operation(void)
{
   struct user_handle_info handle_infos[MAX_SPLIT];
   struct user_handle_info head_handle_info;
   int result = 0;

   memset(handle_infos, 0, sizeof(handle_infos));

   /* get file length and file name */
   CURL *head_handle = curl_easy_init();
   head_handle_info.handle = head_handle;
   head_handle_info.file_fp = NULL;
   curl_easy_setopt(head_handle, CURLOPT_HEADER, 1);
   curl_easy_setopt(head_handle, CURLOPT_NOBODY, 1);
   curl_easy_setopt(head_handle, CURLOPT_VERBOSE, 1);
   curl_easy_setopt(head_handle, CURLOPT_FOLLOWLOCATION , 1);
   curl_easy_setopt(head_handle, CURLOPT_URL, Url);
   curl_easy_setopt(head_handle, CURLOPT_WRITEFUNCTION, write_data);
   curl_easy_setopt(head_handle, CURLOPT_WRITEDATA, &head_handle_info);
   curl_easy_perform(head_handle);
   curl_easy_cleanup(head_handle);

   if( Content_Len == 0){
      if(Content_Len_Suggest == 0){
         fprintf(stderr, "Unkown file size, abort.\n");
         return -1;
      }else{
         Content_Len = Content_Len_Suggest;
      }
   }

   if( If_Count > 0 && Content_Len >= If_Count * 2){
      int i;
      long long range = Content_Len / If_Count;
      size_t suffix_pos;
      char download_file[PATH_MAX + 1] = {'\0'};
      CURLM *multi_handle;

      multi_handle = curl_multi_init();
      /* rewrite here! */
      if( If_Count * range < Content_Len )
         range++;
      strcpy(download_file, Output_File_Name);
      suffix_pos = strlen(Output_File_Name);

      for(i = 0; i < If_Count; i++){
         char range_buffer[256];
         FILE *fp;
         CURL *handle;
         long long r_from, r_to;

         download_file[suffix_pos] = File_Suffix[i];
         download_file[suffix_pos + 1] = '\0';
         if((fp = fopen(download_file, "w+")) == NULL ){
            perror("create file error:");
            result =  -1;
            goto fatal_exit;
         }
         r_from = i * range;
         if( i == If_Count - 1)
            r_to = Content_Len - 1;
         else
            r_to = (i + 1) * range - 1;
         sprintf(range_buffer, "%lld-%lld", r_from, r_to);
         handle = curl_easy_init();
         handle_infos[i].handle = handle;
         handle_infos[i].file_fp = fp;
         curl_easy_setopt(handle, CURLOPT_URL, Url);
         curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
         curl_easy_setopt(handle, CURLOPT_WRITEDATA, &handle_infos[i]);
         curl_easy_setopt(handle, CURLOPT_INTERFACE, Interfaces[i]);
         curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION , 1);
         curl_easy_setopt(handle, CURLOPT_RANGE, range_buffer);
         puts(range_buffer);
         curl_multi_add_handle(multi_handle, handle);
      }

      result = curl_multi_operation(multi_handle, handle_infos);


   }else{
      CURL *handle = curl_easy_init();
      handle_infos[0].handle = handle;
      if((handle_infos[0].file_fp = fopen(Output_File_Name, "w+")) == NULL){
         perror("create file error:");
         result = -1;
         goto fatal_exit;
      }
      //handle_infos[0].sock_fd = 0;
      curl_easy_setopt(handle, CURLOPT_URL, Url);
      curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(handle, CURLOPT_WRITEDATA, &handle_infos[0]);
      curl_easy_perform(handle);
      fclose(handle_infos[0].file_fp);
   }

fatal_exit:
   return result;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   struct user_handle_info *user_info = (struct user_handle_info *)userp;
   if(user_info->file_fp == NULL ){
      size_t len = strlen("Content-Length: ");
      if(Content_Len == 0 && !strncmp((char *)buffer, "Content-Length: ", len)){
         Content_Len = atoll(buffer + len);
         printf("Content_Len = %lld\n", Content_Len);
      }
      return size * nmemb;
   }else{
      return fwrite(buffer, size, nmemb, user_info->file_fp);
   }
}

static int curl_multi_operation(CURLM *multi_handle, struct user_handle_info handle_infos[])
{
   int left_hanle = If_Count;
   fd_set read_fds, write_fds, ex_fds;
   int max_fd;
   int result;

   while(left_hanle > 0){

      int msg_in_queue = -1;
      int running_handlers;
      struct timeval timeout;

#if 0
      int r = 0;
      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);
      FD_ZERO(&ex_fds);
      curl_multi_fdset(multi_handle, &read_fds, &write_fds, &ex_fds, &max_fd);
      if( max_fd > 1){
         timeout.tv_sec = 1;
         timeout.tv_usec = 0;
         r = select(max_fd, &read_fds, &write_fds, &ex_fds, &timeout); /* FIXME: sleep forever.. */
      }else if(max_fd == -1){
         timeout.tv_sec = 0;
         timeout.tv_usec = 10000;
         r = select(0, NULL, NULL, NULL, &timeout); /* sleep for 100  milliseconds */
      }
      if( r > 0)
#else
      timeout.tv_sec = 0;
      timeout.tv_usec = 100;
      select(0, NULL, NULL, NULL, &timeout); /* sleep for 100  milliseconds */
#endif
      curl_multi_perform(multi_handle, &running_handlers);

      do{
         CURLMsg *msgs;
         msgs = curl_multi_info_read(multi_handle, &msg_in_queue);
         if( msgs != NULL && msgs->msg == CURLMSG_DONE){
            int i;
            left_hanle --;
            for( i = 0; i < MAX_SPLIT; i++){
               if( handle_infos[i].handle == msgs->easy_handle)
                  break;
            }
            handle_infos[i].handle = NULL;
            fclose(handle_infos[i].file_fp);
            handle_infos[i].file_fp = NULL;

            curl_multi_remove_handle(multi_handle, msgs->easy_handle);
            curl_easy_cleanup(msgs->easy_handle);
            if( i == MAX_SPLIT){
               fprintf(stderr, "Fatal error! No matched handle\n");
               exit(EXIT_FAILURE);
            }
            if( msgs->data.result == 0){
               printf("Transer via %s is done\n", Interfaces[i]);
               result = -1;
            }else{
               printf("Transer via %s is failed\n", Interfaces[i]);
            }
         }
      }while(msg_in_queue > 0);

   }
   return result;
}
