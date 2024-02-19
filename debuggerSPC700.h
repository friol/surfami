#ifndef DEBUGGERSPC700_H
#define DEBUGGERSPC700_H

#include <string>

struct dbgSPC700info
{
	unsigned char opcode;
	std::string disasm;
	int instrBytes;
};

class debuggerSPC700
{
private:


public:

	debuggerSPC700();
	~debuggerSPC700();


};

#endif
