#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/info.h"

char *strrev(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}

void resetinfo(info* info){
    memset(info->user, 0, MAX_STRING_LENGTH);
    memset(info->password, 0, MAX_STRING_LENGTH);
    memset(info->host, 0, MAX_STRING_LENGTH);
    memset(info->path, 0, MAX_STRING_LENGTH);
    memset(info->filename, 0, MAX_STRING_LENGTH);
    info->port = 21;
}

void parseinfo(info* info, const char *input_info){

    char start[] = "ftp://";
    int state = 0;
    int input_size = strlen(input_info);
    int index = 0;

    for (int i = 0; i < input_size; i++)
    {
        switch (state)
        {
        case 0:
            //In this state we verify the start is ftp://

            if(input_info[i] != start[i]){
                printf("Error reading ftp");
                exit(-1);
            }else{
                if(i == 5)state++;
            }
            break;
        case 1:
            //Guardamos o username neste estado
            if(input_info[i] == '/'){
                //Como não encontramos o simbolo : significa que nao foi introduzido um user nem uma password
                memcpy(info->host,info->user,MAX_STRING_LENGTH); // o host é então aquilo que estivemos a ler até agora
                memset(info->user, 0, MAX_STRING_LENGTH);
                memcpy(info->user,"anonymous",9); // user deafult
                memcpy(info->password, "qualquer-password",17); // passwd default                      
                state = 4;   
                index = 0; //reset the index
            }else if(input_info[i] == ':'){
                //Acabamos de ler o user, vamos começar a ler a password
                state++;
                index = 0; //reset the index
            }else{
                info->user[index++] = input_info[i];
            }
            break;
        case 2:
            //Guardamos a palavra passe neste estado
            if (input_info[i] == '@')
                //Fim da palavra passe
		    	{
		    		state++;
		    		index = 0;
		    	}
            else info->password[index++] = input_info[i];
            break;
        case 3:
            //Guardamos o host
            if (input_info[i] == '/')
			{
				state++;
				index = 0;
			}
            else info->host[index++] = input_info[i];
            break;
        case 4:
            info->path[index++] = input_info[i];
            break;
        default:
            break;
        }
    }

    int maxLength = strlen(info->path);
    int indexPath = 0;
    
    for (int i = maxLength - 1; i >= 0; i--)
    {
        if(info->path[i] == '/'){
            break;
        }else{
            info->filename[indexPath++] = info->path[i];
        }
    }

    strrev(info->filename);
}