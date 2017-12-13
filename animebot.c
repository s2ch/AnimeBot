#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <regex.h>
#include <locale.h>
#include <syscall.h>
#include <curl/curl.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>


int read_line(int sock, char buffer[]){
	int length = 0;
	while (1){
		char data;
		int result = recv(sock, &data, 1, 0);
		if ((result <= 0) || (data == EOF)){
			perror("Connection closed");
			exit(1);
		}
		buffer[length] = data;
		length++;
		if (length >= 2 && buffer[length-2] == '\r' && buffer[length-1] == '\n'){
			buffer[length-2] = '\0';
			//printf("%s\n", buffer);
			return length;
		}
	}
}


char *get_config(char name[]){
	char *value = malloc(1024);
	FILE *configfile = fopen("config.txt", "r");
	value[0] = '\0';
	if (configfile != NULL){
		while (1){
			char configname[1024];
			char tempvalue[1024];
			int status = fscanf(configfile, " %1023[^= ] = %s ", configname, tempvalue); //Parse key=value
			if (status == EOF){
				break;
			}
			if (strcmp(configname, name) == 0){
				strncpy(value, tempvalue, strlen(tempvalue)+1);
				break;
			}
		}
		fclose(configfile);
	}
	return value;
}

char *get_prefix(char line[]){
	char *prefix = malloc(512);
	char clone[512];
	strncpy(clone, line, strlen(line)+1);
	if (line[0] == ':'){
		char *splitted = strtok(clone, " ");
		if (splitted != NULL){
			strncpy(prefix, splitted+1, strlen(splitted)+1);
		}else{
			prefix[0] = '\0';
		}
	}else{
		prefix[0] = '\0';
	}
	return prefix;
}

char *get_username(char line[]){
	char *username = malloc(512);
	char clone[512];
	strncpy(clone, line, strlen(line)+1);
	if (strchr(clone, '!') != NULL){
		char *splitted = strtok(clone, "!");
		if (splitted != NULL){
			strncpy(username, splitted+1, strlen(splitted)+1);
		}else{
			username[0] = '\0';
		}
	}else{
		username[0] = '\0';
	}
	return username;
}

char *get_command(char line[]){
	char *command = malloc(512);
	char clone[512];
	strncpy(clone, line, strlen(line)+1);
	char *splitted = strtok(clone, " ");
	if (splitted != NULL){
		if (splitted[0] == ':'){
			splitted = strtok(NULL, " ");
		}
		if (splitted != NULL){
			strncpy(command, splitted, strlen(splitted)+1);
		}else{
			command[0] = '\0';
		}
	}else{
		command[0] = '\0';
	}
	return command;
}

char *get_last_argument(char line[]){
	char *argument = malloc(512);
	char clone[512];
	strncpy(clone, line, strlen(line)+1);
	char *splitted = strstr(clone, " :");
	if (splitted != NULL){
		strncpy(argument, splitted+2, strlen(splitted)+1);
	}else{
		argument[0] = '\0';
	}
	return argument;
}

char *get_argument(char line[], int argno){
	char *argument = malloc(512);
	char clone[512];
	strncpy(clone, line, strlen(line)+1);
	
	int current_arg = 0;
	char *splitted = strtok(clone, " ");
	while (splitted != NULL){
		if (splitted[0] != ':'){
			current_arg++;
		}
		if (current_arg == argno+1){
			strncpy(argument, splitted, strlen(splitted)+1);
			return argument;
		}
		splitted = strtok(NULL, " ");
	}
	
	if (current_arg != argno){
		argument[0] = '\0';
	}
	return argument;
}

void set_nick(int sock, char nick[]){
	char nick_packet[512];
	sprintf(nick_packet, "NICK %s\r\n", nick);
	send(sock, nick_packet, strlen(nick_packet), 0);
}

void send_user_packet(int sock, char nick[]){
	char user_packet[512];
	sprintf(user_packet, "USER %s 0 * :%s\r\n", nick, nick);
	send(sock, user_packet, strlen(user_packet), 0);
}

void join_channel(int sock, char channel[]){
	char join_packet[512];
	sprintf(join_packet, "JOIN %s\r\n", channel);
	send(sock, join_packet, strlen(join_packet), 0);
}

void send_pong(int sock, char argument[]){
	char pong_packet[512];
	sprintf(pong_packet, "PONG :%s\r\n", argument);
	send(sock, pong_packet, strlen(pong_packet), 0);
}

void send_message(int sock, char to[], char message[]){
	char message_packet[512];
	sprintf(message_packet, "PRIVMSG %s :%s\r\n", to, message);
	send(sock, message_packet, strlen(message_packet), 0);
}

/* size_t write_data(char *data, size_t size, size_t nmemb, char *buffer){ */
/* 	strcpy(buffer, data); */
/* 	return	size * nmemb; */
/* } */

int main() {
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1){
		perror("Could not create socket");
		exit(1);
	}
	
	char *ip = get_config("server");
	char *port = get_config("port");


	struct sockaddr_in server;
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));

	free(ip);
	free(port);

	if (connect(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0){
		perror("Could not connect");
		exit(1);
	}
	
	char *nick = get_config("nick");
	char *channels = get_config("channels");

	set_nick(socket_desc, nick);
	send_user_packet(socket_desc, nick);
	join_channel(socket_desc, channels);

	free(nick);
	free(channels);

	regex_t webm;
	char *r_webm="^!(webm|шебм|вебм) [a-z]+";
	size_t nmatch=0;
	regmatch_t *pmatch;
	regcomp(&webm, r_webm, REG_ICASE | REG_EXTENDED);

	while (1){
		char line[512];
		read_line(socket_desc, line);
		char *prefix = get_prefix(line);
		char *username = get_username(line);
		char *command = get_command(line);
		char *argument = get_last_argument(line);

		if (strcmp(command, "PING") == 0)
			send_pong(socket_desc, argument); 
		else if (strcmp(command, "PRIVMSG") == 0){
			char *channel = get_argument(line, 1);

			/* Поиск WEBM  */
			if(regexec(&webm, argument, nmatch, pmatch, 0) == 0){
				CURL *curl_f = curl_easy_init();
				CURL *curl_json = curl_easy_init();
				JsonParser *parser;
				JsonNode *json_root;
				GError *error;
				int i = 0;
				char *ar = argument;
				while(*ar != ' ')
					ar++;
				ar++;
				char board[8];
				for(i = 0; islower(*ar); i++)
					board[i] = *ar++;
				board[i] = '\0';
				printf("board = %s\n", board);

				char *url_json;
				asprintf(&url_json, "%s%s%s", "https://2ch.hk/", board, "/catalog.json");
				printf("url_json = %s\n", url_json);
				FILE *fp_json;
				fp_json = fopen("j.json", "w");
				/* Пишет json доски в файл j.json */
				if(curl_json){
					CURLcode res;
					curl_easy_setopt(curl_json, CURLOPT_URL, url_json);
					curl_easy_setopt(curl_json, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:57.0) Gecko/20100101 Firefox/57.0");
					curl_easy_setopt(curl_json, CURLOPT_WRITEFUNCTION, NULL);
					curl_easy_setopt(curl_json, CURLOPT_WRITEDATA, fp_json);
					res = curl_easy_perform(curl_json);
  
				}
				fclose(fp_json);
				parser = json_parser_new ();
  
				error = NULL;
				json_parser_load_from_file (parser, "j.json", &error);
				json_root = json_parser_get_root(parser);
  
				JsonObject *object;
				JsonArray *array_member;
				object = json_node_get_object(json_root); 
				array_member = json_object_get_array_member(object, "threads");
  
				JsonNode *node;
				JsonObject *obj;
				JsonNode *node_files;
				JsonObject *obj_files;
				char thread[12];
				JsonArray *array_files;
				/* Парсит json, номер треда помещает в thread, если не находит, то ???*/
				for(i = 0; i < json_array_get_length(array_member); i++){
					node = json_array_get_element(array_member, i);
					obj = json_node_get_object(node);
					array_files = json_object_get_array_member(obj, "files");
					node_files = json_array_get_element(array_files, 0);
					obj_files = json_node_get_object(node_files);
					if(strcasestr(json_object_get_string_member(obj_files, "name"), "webm")){
						strcpy(thread, json_object_get_string_member(obj, "num"));
						break;
					}
			    
			      
				}
			  
				g_object_unref (parser);
				/* json_array_unref(array_member); */
				/* json_object_unref(object); */
				/* json_object_unref(obj); */
				/* json_node_free(json_root); */
				/* json_node_free(node); */
  
				//char url_html[] = "https://2ch.hk/a/res/4099203.html";
				char *url_html;
				asprintf(&url_html, "%s%s%s%s%s", "https://2ch.hk/", board, "/res/", thread, ".html");
				printf("url_html = %s\n", url_html);
			 
				FILE *f;
				char *p;
				f = fopen("f.html", "w+");
				/* Сохраняет тред в f.html */
				if(curl_f){
					CURLcode res;
					curl_easy_setopt(curl_f, CURLOPT_URL, url_html);
					curl_easy_setopt(curl_f, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:57.0) Gecko/20100101 Firefox/57.0");
					curl_easy_setopt(curl_f, CURLOPT_WRITEFUNCTION, NULL);
					curl_easy_setopt(curl_f, CURLOPT_WRITEDATA, f);
					res = curl_easy_perform(curl_f);
			    
				}
			  
				rewind(f);

				char *ptr = NULL;
				size_t len = 0;

				char **webmsarray = malloc(512);
				i = 0;
				int webmcount = 0;

				/* ТУТ ПАДАЕТ */
				/* парсит html, вебм урлы сохраняет в webmsarray */
				while(getline(&ptr, &len, f) > 0){
			    
					if((p = strstr(ptr, ".webm")) != NULL &&
					   strstr(ptr, "desktop") == NULL){
						while(*p != '"')
							p++;
						*p = '\0';
						while(*ptr != '"')
							ptr++;
						++ptr;
						asprintf(&webmsarray[i++], "%s%s", "https://2ch.hk", ptr); 
						webmcount++;
						
						/* без этого условия падало с ошибкой memory corruped */
						if(webmcount > 50)
							break; 
					} 
				}
			  
				printf("webmcount = %d\n", webmcount);

				/* печатает рандомную вебм из webmarray в чат */
				if(webmcount > 0){
					srand(time(NULL));
					int random_webm = rand() % webmcount;
					printf("webmsarray = %s\n", webmsarray[random_webm]); 
					send_message(socket_desc, channel, webmsarray[random_webm]);
				}
				
				for(i = 0; i < webmcount; i++)
					free(webmsarray[i]);
				free(webmsarray);
				fclose(f);
				free(url_json);
				free(url_html);
				curl_easy_cleanup(curl_json);
				curl_easy_cleanup(curl_f); 
			}
			
			free(channel);
		}

		free(prefix);
		free(username);
		free(command);
		free(argument);
	}
	regfree(&webm);

}
