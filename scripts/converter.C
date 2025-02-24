#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
// #include <bitset>
#include <cmath>

using namespace std;

ifstream filein;
ofstream fileheader;
ofstream filepayload;

int read_header()
{
	int           nlines;
	char          data[16];
	unsigned int* data_int = (unsigned int*)data;
	for(int i = 0; i < 9; i++)
	{
		for(int j = 0; j < 2; j++)
		{
			filein.read(data, 4);
			cout << data << endl;
			if(filein.eof())
				return -1;
			fileheader << setfill('0') << setw(8) << hex << data_int[0] << " ";
			if(i == 0 && j == 0)
				nlines = floor((data_int[0] - 72) / 32);
		}
		fileheader << endl;
	}
	return nlines;
}

void read_event(int nlines)
{
	filepayload << "E" << dec << nlines << endl;

	char          data[16];
	unsigned int* data_int = (unsigned int*)data;
	for(int i = 0; i < nlines; i++)
	{
		string b = "";
		for(int j = 0; j < 8; j++)
		{
			filein.read(data, 4);
			filepayload << setfill('0') << setw(8) << hex << data_int[0];
		}
		filepayload << endl;
	}
	filepayload << "N" << endl;

	return;
}

int converter(string run_num)
{
	string filetype = "macroOutput";

	string file_in = filetype + "_" + run_num + ".bin";
	cout << "Opening file " << std::string(getenv("OTSDAQ_DATA")) + "/" + file_in << endl;

	filein.open(file_in.c_str(), ios::binary);

	string file_header =
	    std::string(getenv("OTSDAQ_DATA")) + "/" + "header_" + run_num + ".txt";
	fileheader.open(file_header.c_str());
	string file_payload =
	    std::string(getenv("OTSDAQ_DATA")) + "/" + "payload_" + run_num + ".txt";
	filepayload.open(file_payload.c_str());

	while(true)
	{
		int nlines = read_header();
		if(nlines == -1)
			break;
		cout << nlines << " LINES" << endl;
		read_event(nlines);
	}

	return 0;
}
