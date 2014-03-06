#include <assert.h>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "parser.h"

Parser::Parser() : pregInitialized(false) {
}

ParsedFile::parsed_file_ptr Parser::parse(std::istream& instream) {
	ParsedFile::parsed_file_ptr parsedFile = ParsedFile::parsed_file_ptr(new ParsedFile());
	std::string line;

	while(std::getline(instream, line)) {
		std::istringstream iss(line);

		parseLine(line, parsedFile);
	}

	return parsedFile;
}

void Parser::parseLine(std::string& line, ParsedFile::parsed_file_ptr parsedFile) {
	// 1 = timestamp
	// 2 = session? (stuff before @context:line)
	// 3 = context
	// 4 = line in context
	// 5 = function/method
	// 6 = channel id
	int res;

	if(!pregInitialized) {
		if((res = regcomp(&preg, "\\[([^\\[]+)\\][ ]+\\-\\- Executing \\[([^@]+)@([^:]+):([0-9]+)\\] ([^\\(]+)\\(\"([^\"]+)", REG_EXTENDED)) != 0) {
			char error[BUFSIZ];
			regerror(res, &preg, error, BUFSIZ);
			std::cerr << error << std::endl;
			throw std::string("RegEx compilation failed");
			pregInitialized = true;
		}
	}

	if(line.size() < 24) // there isnt even a timestamp. Just ignore it.
		return;

	std::string other = line.substr(24);

	regmatch_t pmatch[10];

	res = regexec(&preg, line.c_str(), 10, pmatch, REG_NOTBOL);
	if(res == REG_NOMATCH) {
		// This line didnt match our regex. As a fallback, determine the known channelids and
		// see if the line contains one of them. 
		bool foundChannel = false;

		std::string timestamp = line.substr(1,22);

		ParsedFile::channels_t channels = parsedFile->getChannels();
		
		for(ParsedFile::channels_t::iterator i = channels.begin(); i != channels.end(); ++i) {
			if(line.find(i->first) != std::string::npos) {
				parsedFile->addChannelLine(timestamp, i->first, other);
				foundChannel = true;
				break;
			}
		}
		
		if(!foundChannel) {
			// We didnt find any channel matching, fall back to just "line"
			parsedFile->addLine(timestamp, other);
		}
		return;
	}

	int len;
	char result[BUFSIZ];
	std::string timestamp;
	std::string channelId;

	for (int i = 0; pmatch[i].rm_so != -1; i++) {
		len = pmatch[i].rm_eo - pmatch[i].rm_so;
		memcpy(result, line.c_str() + pmatch[i].rm_so, len);
		result[len] = 0;

		if(i == 1)
			timestamp = std::string(result);
		else if(i == 6)
			channelId = std::string(result);
	}
	regfree(&preg);

	parsedFile->addChannelLine(timestamp, channelId, other);
}

