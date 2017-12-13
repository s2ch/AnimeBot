# AnimeBot
A very simple IRC bot written in C. Its primary purpose is posting `.webm` files on channels.

# Compiling
There are multiple ways to compile AnimeBot.

1. Running `make` or `make compile` will compile the code and leave an executable called _animebot_.
2. Running `make run` will compile the code and run the executable in one step.
3. Running `make test` will compile the code and perform memory tests using _valgrind_.
4. Running `make clean` will remove the executable and only leave the code files.

# How to Use
Type `!webm <board_name>` and enjoy.

# The Config File
The program can be configured by editing the config.txt file. The file is in the `key = value` format. The settings are:

1. **server**: Used to set the server ip to connect to. Example: `server = 195.154.200.232`
2. **port**: Used to set the port of the server. The default irc port is 6667. Example: `port = 6667`
3. **nick**: Used to set the nick of the bot. Example: `nick = AnimeBot`
4. **channels**: Used to set the channel list for the bot, comma seperated. Example: `channels = #s2ch, <#channel>`
