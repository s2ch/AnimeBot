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

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

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

	struct MemoryStruct chunk;
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:57.0) Gecko/20100101 Firefox/57.0");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	
	while (1){
		char line[512];
		read_line(socket_desc, line);
		char *prefix = get_prefix(line);
		char *username = get_username(line);
		char *command = get_command(line);
		char *argument = get_last_argument(line);

		chunk.memory = malloc(1);
		chunk.size = 0;
		
		if (strcmp(command, "PING") == 0)
			send_pong(socket_desc, argument); 
		else if (strcmp(command, "PRIVMSG") == 0){
			char *channel = get_argument(line, 1);

			/* Поиск WEBM  */
			if(regexec(&webm, argument, nmatch, pmatch, 0) == 0){
				JsonParser *parser;
				JsonNode *json_root;
				GError *error = NULL;
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

				char *url;
				asprintf(&url,
						 "%s%s%s",
						 "https://2ch.hk/",
						 board,
						 "/catalog.json");
				
				printf("url_json = %s\n", url);
				
				curl_easy_setopt(curl, CURLOPT_URL, url); 
				curl_easy_perform(curl);

				parser = json_parser_new ();

				json_parser_load_from_data(parser,
										   chunk.memory,
										   strlen(chunk.memory),
										   &error);
				json_root = json_parser_get_root(parser);
  
				JsonObject *object;
				JsonArray *array_member;
				object = json_node_get_object(json_root); 
				array_member = json_object_get_array_member(object,
															"threads");
  
				JsonNode *node;
				JsonObject *obj;
				JsonNode *node_files;
				JsonObject *obj_files;
				char thread[12];
				JsonArray *array_files;
				/* Парсит json, номер треда помещает в thread */
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
				
				asprintf(&url,
						 "%s%s%s%s%s",
						 "https://2ch.hk/",
						 board,
						 "/res/",
						 thread,
						 ".html");
				
				printf("url_html = %s\n", url);

				curl_easy_setopt(curl, CURLOPT_URL, url); 
				curl_easy_perform(curl);

				char **webmsarray = malloc(sizeof(char **));
				i = 0;
				int webmcount = 0;

				/* парсит html, вебм урлы сохраняет в webmsarray */ 
			    char *p = chunk.memory;
					while((p = strstr(p, ".webm\""))){
						char *p_ptr = p;
						while(*p_ptr != '"')
							p_ptr--;
						++p_ptr;
						while(*p != '"')
							p++;
						*p = '\0'; 
						if(strstr(p_ptr, "/src/")){
							webmsarray = realloc(webmsarray, sizeof(char *) * (i+1));
							asprintf(&webmsarray[i++], "%s%s", "https://2ch.hk", p_ptr); 
							webmcount++;
						}
						p++; 
					} 
			  
				printf("webmcount = %d\n", webmcount);

				/* печатает рандомную вебм из webmarray в чат */
				if(webmcount > 0){
					srand(time(NULL));
					int random_webm = rand() % webmcount;
					printf("webmsarray = %s\n", webmsarray[random_webm]); 
					send_message(socket_desc, channel, webmsarray[random_webm]);
				}else
					send_message(socket_desc, channel, "Я ничего не нашла, прости сенпаи T_T");
				
				for(i = 0; i < webmcount; i++)
					free(webmsarray[i]);
					
				free(webmsarray); 
				free(url); 
			}
			
			free(channel);
			free(chunk.memory);
		}

		free(prefix);
		free(username);
		free(command);
		free(argument);
	}
	regfree(&webm);
	curl_easy_cleanup(curl);

}
