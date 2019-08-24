#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdint.h>
#include "optionparser.h"
using namespace std;

uint8_t xorall(uint8_t *bytebuffer, int N) {
	uint8_t r=0;
	for (int i=0; i<N; i++)
		r^=bytebuffer[i];
	return r;
}

int dec2int(char x) {
	if (x>='0' && x<='9') return x-'0';
	return -1;
}

int hex2int(char x) {
	if (x>='0' && x<='9') return x-'0';
	if (x>='A' && x<='F') return x-'A'+10;
	if (x>='a' && x<='f') return x-'a'+10;
	return -1;
}

vector<char> parseNameZX(string given) {
	vector<char> result;
	for (int src=0; src<given.length(); )
		if (given[src]!='\\') result.push_back(given[src++]);
		else {
			src++;
			if (given[src]=='\\') result.push_back(given[src++]);
			else if (given[src]=='#') { // hexadecimal
				if (hex2int(given[src+1])>=0 && hex2int(given[src+2])>=0) {
					result.push_back(hex2int(given[src+1])*16+hex2int(given[src+2]));
				} else {
					cout << "  error: \\#" << given[src+1] << given[src+2]
						<< " is not a valid hexadecimal code.\n" << endl;
					result.clear();
					return result;
				}
				src+=2;
			} else if (given[src]>='0' && given[src]<='2') { // decimal
				if (dec2int(given[src+1])>=0 && dec2int(given[src+2])>=0) {
					int val=dec2int(given[src])*100+dec2int(given[src+1])*10+dec2int(given[src+2]);
					if (val<=255) {
						result.push_back(val); src+=2;
					} else {
						cout << "  error: \\" << given[src] << given[src+1] << given[src+2]
							<< " is not a valid decimal code.\n" << endl;
						result.clear();
						return result;
					}
				} else {
					cout << "  error: \\" << given[src] << given[src+1] << given[src+2]
						<< " is not a valid decimal code.\n" << endl;
					result.clear();
					return result;
				}
				src+=2;
			} else { // undefined escape sequence
				result.push_back('\\');
				result.push_back(given[src]);
				src++;
			}
		}
	result.resize(10, ' ');
	return(result);
}

enum  optionIndex { UNKNOWN, HELP, BASIC, CODE, SCREEN, RAW };
const option::Descriptor usage[] = {
	{UNKNOWN, 0,	"",	"",	option::Arg::None,	"  a tool to create tape images out of binary files\n\n"
												"  usage:\n"
												"    bin2tap [-r] input.bin output.tap\n"
												"      creates a headerless tape block\n"
												"    bin2tap -b input.bin output.tap zxname [start]\n"
												"      creates a basic file that autostarts at line 'start'\n"
												"    bin2tap -c input.bin output.tap zxname start\n"
												"      creates a code file that loads at address 'start'\n"
//												"    bin2tap -s input.bin output.tap zxname\n"
//												"      creates a screen$ file\n\n"
												"  options:" },
	{HELP, 0,	"h",	"help",	option::Arg::None, "    -h, --help  \tprint usage and exit."},
	{BASIC, 0,	"b",	"basic",	option::Arg::None, "    -b, --basic  \tcreate basic *.tap file."},
	{CODE, 0,	"c",	"code",	option::Arg::None, "    -c, --code  \tcreate code *.tap file."},
//	{SCREEN, 0,	"s",	"screen",	option::Arg::None, "    -s, --screen  \tcreate screen$ *.tap file."},
	{RAW, 0,	"r",	"raw",	option::Arg::None, "    -r, --raw  \tcreate a raw block, i.e. do not generate block id and checksum."},
	{0,0,0,0,0,0}
};

int main(int argc, char *argv[]) {

	argc--; argv++; // always skip program name argv[0]
	option::Stats	stats(usage, argc, argv);
	option::Option*	options = new option::Option[stats.options_max];
	option::Option*	buffer = new option::Option[stats.buffer_max];
	option::Parser	parse(usage, argc, argv, options, buffer);
	if (parse.error()) return 1;

	if (argc>0) cout << endl;
	cout << "  bin2tap ver.0.3 by introspec (" << __DATE__ << ")" << endl << endl;
	if (argc == 0 || options[HELP]) {
		option::printUsage(std::cout, usage);
		return 0;
	}

	string argin, argout;

	//cout << "[" << parse.optionsCount() << " " << parse.nonOptionsCount() << "]" << endl;

	// the case of headerless *.tap file
	if ((parse.optionsCount()==0 || (parse.optionsCount()==1 && options[RAW])) && parse.nonOptionsCount() == 2) {
		string argin = string(parse.nonOption(0)), argout = string(parse.nonOption(1));

		std::ifstream filein(argin.c_str(), std::ifstream::binary);
		if (filein) {
			// measure input file length
			filein.seekg(0, filein.end);
			int lenin = static_cast<int>(filein.tellg());
			filein.seekg(0, filein.beg);

			//cout << lenin << endl;

			// copy the file
			if (lenin<=65535 && options[RAW]) {
				// copy data verbatim
				uint8_t bytebuffer[65535];
				std::ofstream fileout(argout.c_str(), std::ifstream::binary);
				filein.read(reinterpret_cast<char *>(bytebuffer), lenin);

				fileout.write(reinterpret_cast<char *>(&lenin), 2); // data block length
				fileout.write(reinterpret_cast<char *>(bytebuffer), lenin); // data block itself
				fileout.close();
			} else if (lenin<=65535-2 && !options[RAW]) {
				// create an appropriate data block
				uint8_t bytebuffer[65535];
				bytebuffer[0] = 0xFF;
				std::ofstream fileout(argout.c_str(), std::ifstream::binary);
				filein.read(reinterpret_cast<char *>(&bytebuffer[1]), lenin);
				bytebuffer[lenin+1] = xorall(bytebuffer, lenin+1);

				lenin+=2;
				fileout.write(reinterpret_cast<char *>(&lenin), 2); // data block length
				fileout.write(reinterpret_cast<char *>(bytebuffer), lenin); // data block itself
				fileout.close();
			} else {
				cout << "  error: the input file is too long." << endl; return 1;
			}

			filein.close();
			return 0;
		} else {
			cout << "  error: cannot open the input file." << endl; return 1;
		}
	}

	// the case of a basic *.tap file
	if ((parse.optionsCount()==1 && options[BASIC]) && parse.nonOptionsCount() >= 3 && parse.nonOptionsCount() <= 4) {
		string argin = string(parse.nonOption(0)), argout = string(parse.nonOption(1));
		string tapname = string(parse.nonOption(2));
		int line=-1;
		if (parse.nonOptionsCount()==4) {
			line=atoi(parse.nonOption(3)); // will be 0 for non-numeric inputs
		}

		std::ifstream filein(argin.c_str(), std::ifstream::binary);
		if (filein) {
			// measure input file length
			filein.seekg(0, filein.end);
			int lenin = static_cast<int>(filein.tellg());
			filein.seekg(0, filein.beg);

			if (lenin<=65535-2) {
				// set up the code header
				uint8_t bytebuffer[65535];
				bytebuffer[0]=0; // means "header"
				bytebuffer[1]=0; // means "basic"
				vector<char> parsedname=parseNameZX(tapname);
				for (int i=0; i<10; i++) {
					bytebuffer[i+2]=parsedname[i];
				}
				bytebuffer[12]=reinterpret_cast<uint8_t *>(&lenin)[0];
				bytebuffer[13]=reinterpret_cast<uint8_t *>(&lenin)[1];
				if (line>=0) {
					bytebuffer[14]=reinterpret_cast<uint8_t *>(&line)[0];
					bytebuffer[15]=reinterpret_cast<uint8_t *>(&line)[1];
				} else {
					bytebuffer[14]=0;
					bytebuffer[15]=0x80;
				}
				bytebuffer[16]=bytebuffer[12]; // no variables are assumed here
				bytebuffer[17]=bytebuffer[13]; // may generalize in the future
				bytebuffer[18] = xorall(bytebuffer, 18);

				// save header
				std::ofstream fileout(argout.c_str(), std::ifstream::binary);
				int hdrlen=19;
				fileout.write(reinterpret_cast<char *>(&hdrlen), 2); // header length
				fileout.write(reinterpret_cast<char *>(bytebuffer), hdrlen); // header itself

				// save data
				bytebuffer[0] = 0xFF;
				filein.read(reinterpret_cast<char *>(&bytebuffer[1]), lenin);
				bytebuffer[lenin+1] = xorall(bytebuffer, lenin+1);

				lenin+=2;
				fileout.write(reinterpret_cast<char *>(&lenin), 2); // data block length
				fileout.write(reinterpret_cast<char *>(bytebuffer), lenin); // data block itself
				fileout.close();
			} else {
				cout << "  error: the input file is too long." << endl; return 1;
			}
		} else {
			cout << "  error: cannot open the input file." << endl; return 1;
		}
	}

	// the case of a code *.tap file
	if ((parse.optionsCount()==1 && options[CODE]) && parse.nonOptionsCount() == 4) {
		string argin = string(parse.nonOption(0)), argout = string(parse.nonOption(1));
		string tapname = string(parse.nonOption(2));
		int addr = atoi(parse.nonOption(3)); // will be 0 for non-numeric inputs

		std::ifstream filein(argin.c_str(), std::ifstream::binary);
		if (filein) {
			// measure input file length
			filein.seekg(0, filein.end);
			int lenin = static_cast<int>(filein.tellg());
			filein.seekg(0, filein.beg);

			if (lenin<=65535-2) {
				// set up the code header
				uint8_t bytebuffer[65535];
				bytebuffer[0]=0; // means "header"
				bytebuffer[1]=3; // means "code"

				vector<char> parsedname=parseNameZX(tapname);
				for (int i=0; i<10; i++) {
					bytebuffer[i+2]=parsedname[i];
				}
				bytebuffer[12]=reinterpret_cast<uint8_t *>(&lenin)[0];
				bytebuffer[13]=reinterpret_cast<uint8_t *>(&lenin)[1];
				bytebuffer[14]=reinterpret_cast<uint8_t *>(&addr)[0];
				bytebuffer[15]=reinterpret_cast<uint8_t *>(&addr)[1];
				bytebuffer[16]=0;
				bytebuffer[17]=0x80;
				bytebuffer[18] = xorall(bytebuffer, 18);

				// save header
				std::ofstream fileout(argout.c_str(), std::ifstream::binary);
				int hdrlen=19;
				fileout.write(reinterpret_cast<char *>(&hdrlen), 2); // header length
				fileout.write(reinterpret_cast<char *>(bytebuffer), hdrlen); // header itself

				// save data
				bytebuffer[0] = 0xFF;
				filein.read(reinterpret_cast<char *>(&bytebuffer[1]), lenin);
				bytebuffer[lenin+1] = xorall(bytebuffer, lenin+1);

				lenin+=2;
				fileout.write(reinterpret_cast<char *>(&lenin), 2); // data block length
				fileout.write(reinterpret_cast<char *>(bytebuffer), lenin); // data block itself
				fileout.close();
			} else {
				cout << "  error: the input file is too long." << endl; return 1;
			}
		} else {
			cout << "  error: cannot open the input file." << endl; return 1;
		}
	}

	delete[] options;
	delete[] buffer;
	return 0;
}
