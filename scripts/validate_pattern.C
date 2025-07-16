
// -- read and validate binary file with variable size and +1 counter
//
//  to account for different number of ROCs involved:
//  - change file_in/file_out,
//  - LINK_NO, FIRST_LINK
//
//  > name file to analyze    calo_trk_EVTNO.bin
//  > compile with            /usr/bin/g++ ryan_dtc_validate.cpp -o ryan_dtc_validate
//  > run as                  ./ryan_dtc_validate EVTNO
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

int validate_pattern(int ievtno, string file_name) {
	// file names & locations
	// string file_in = "/sudaq/DAQData/TriggerData/MactrisData/l1_rawdata_run" + run +
	// ".bin";

	string   file_in  = file_name + ".bin";
	string   file_out = "error_" + file_name + ".txt";
	ifstream fin(file_in.c_str(), ofstream::binary);
	ofstream fout(file_out.c_str());

	if(!fin.is_open()) {
		cout << "ERROR - cannot open input file" << endl;
		return -1;
	} else {  // BINARY data file found: define&initialize all variables and start WHILE
		      // loop until end of file is found
		cout << "******************************************************" << endl;
		cout << "Opening input file " << file_in.c_str() << endl << endl;
		cout << "Errors saved in file " << file_out.c_str() << endl << endl;

		// int DEF_STATUS = 0x155;
		int DEF_STATUS = 0x101;

		char          data[16];
		unsigned int* data_int = (unsigned int*)data;

		// counter for progress updates
		int evtcount = 0;   // local event counter (starts ar zero for every run: need to
		                    // add EVTOFFSET)
		int wordcount = 0;  // 32-bit word counter
		int hexcount  = 0;  // byte counter

		int evt_start     = 1;
		int check_header  = 0;
		int check_payload = 0;

		int evt_offset = 0;

		int          is_timeout         = 0;
		int          ntimeout           = 0;
		int          ntimeout_with_dreq = 0;
		unsigned int prev_timeout_data;

		int word_counter      = 0;  // counter of words in event
		int head_counter      = 0;  // counter of words in event header
		int link_head_counter = 0;  // counter of word in data header
		int payload_counter   = 0;  // counter of words in payload

		int allFs = 0;  // switches between header words with all Fs vs with 01
		// 1 link
		// const int LINK_NO = 1;
		// const int FIRST_LINK = 0;
		// const int EMPTY_LINKS = 5;

		// 2 links
		const int LINK_NO     = 2;
		const int FIRST_LINK  = 0;
		const int EMPTY_LINKS = 4;

		int ilink = FIRST_LINK;

		// int SKIP = 0x3A0;
		int SKIP = 0;

		int SPECIAL_FIRST_HIT = 1;
		int ihit              = SPECIAL_FIRST_HIT;

		int DEBUG = 0;

		// sizes in 32-bit words;
		int evt_size;      // total event (N links) size
		int link_size;     // size for one non-empty link
		int payload_size;  // payload size

		int lsb_word, msb_word;
		int low_word, high_word;

		int word_in_payload;

		const int LINK_HEAD_SIZE = 4;  // link data header in 32-bit words

		int hit_in[64] = {1,  2, 3, 0, 0,  0, 7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 0, 20, 21, 22, 12, 13, 11, 12, 0,  0,  8,  4,  12, 11, 12, 13,
		                  16, 6, 3, 1, 12, 0, 16, 17, 18, 19, 12, 1,  12, 12, 11, 11, 0, 0,  0,  0,  13, 14, 10, 13, 11, 14, 14, 15, 8,  9,  10, 32};

		unsigned int last_data;
		unsigned int prev_data       = -1;
		unsigned int start_prev_data = -1;
		unsigned int exp_data_int;
		unsigned int first_data_error;

		int trg_no;
		int prev_trg_no = -1;

		int err_cnt             = 0;
		int hit_err_cnt         = 0;
		int head_err_cnt        = 0;
		int trig_err_cnt        = 0;
		int link_err_cnt        = 0;
		int payload_err_cnt     = 0;
		int payload_evt_err_cnt = 0;

		int is_head_error      = 0;
		int is_trigger_error   = 0;
		int is_hold_error      = 0;
		int is_link_head_error = 0;
		int is_payload_error   = 0;

		// process entire contents of file
		while(!fin.eof()) {
			// reads in 32 bits (or 4 bytes) at a time:
			//   16 LSB are first in dump, 16 MSB are secnd in dump
			fin.read(data, 4);

			lsb_word = data_int[0] & 0xFFFF;
			msb_word = (data_int[0] >> 16);

			// line number from "hexdump"
			hexcount = wordcount * 4;
			wordcount++;

			// cout << " data_int[0] = 0x"<<hex<<data_int[0];
			// cout << " at hexcount = 0x"<<hex<<hexcount<<dec<<endl;

			// skip first words
			if(hexcount < SKIP)
				continue;

			if(evt_start) {
				// cout << "inside evt_start  at hexcount = 0x"<<hex<<hexcount<<dec<<endl;
				// exit when the word is not updating
				if(data_int[0] == last_data)
					break;

				evt_start = 0;
				ilink     = 0;

				// predicted event size: 2 links with payload + 3 link with only header
				payload_size = hit_in[ihit] * 8;
				link_size    = payload_size + LINK_HEAD_SIZE;
				evt_size     = LINK_NO * link_size + EMPTY_LINKS * LINK_HEAD_SIZE;

				evtcount++;
				// if ((evtcount-1)>=3135 && (evtcount-1)<3136) DEBUG = 1;
				// else DEBUG = 0;

				if(DEBUG) {
					cout << endl << " At evtcount " << evtcount << " payload_size=" << payload_size << " link_size=" << link_size << " and evt_size=" << evt_size << endl;
				}
				// cout<< " Read data_int=0x"<<hex<<data_int[0]<<" and
				// prev_data=0x"<<prev_data<<dec<<endl;

				// DEBUG = 1;
				// if (evtcount>20) return 0;
				// if (evtcount>40000) break;

				word_in_payload = hit_in[ihit] * 8;
				ihit++;
			}  // end if (evt_start)

			// reset simulated hit index
			if(ihit == 64)
				ihit = 0;

			word_counter++;

			//
			// decide if packet header is to be checked
			if(check_payload == 0 && ilink < (LINK_NO + EMPTY_LINKS))
				check_header = 1;

			if(check_header) {
				link_head_counter++;

				// cout<<endl<<" Checking header word "<<link_head_counter<<" for link
				// "<<ilink<<" of event "<<evtcount<<endl;

				// find trigger number to use for each link in the event
				if(ilink == 0 && link_head_counter == 2)
					trg_no = msb_word;
				if(ilink == 0 && link_head_counter == 3) {
					trg_no = trg_no + (lsb_word << 16);
					if(DEBUG)
						cout << "  Event " << evtcount << " has timestamp 0x" << hex << trg_no << dec << endl;
				}

				// first DATA HEADER WORD contains VALID word + overall LINK SIZE in units
				// of bites
				if(link_head_counter == 1) {
					if(DEBUG)
						cout << "  starting link header check for ilink " << ilink << " of event " << evtcount << dec << endl;

					if(ilink < LINK_NO) {
						low_word = link_size * 4;
						// high_word = (0x8050 + 0x800 + ilink*0x100);
						high_word = (0x8050 + ilink * 0x100);
					} else if(ilink <= 2) {
						low_word  = LINK_HEAD_SIZE * 4;
						high_word = (0x8050 + 0x1000 + ilink * 0x100);
					} else {
						low_word  = LINK_HEAD_SIZE * 4;
						high_word = (0x8050 + ilink * 0x100);
					}

				}  // end if (link_head_counter == 1)

				// second DATA HEADER WORD contains TRIGGER number[15:0] + no. of DTC
				// packets (= number of hits*4)
				if(link_head_counter == 2) {
					if(ilink < LINK_NO) {
						low_word = payload_size / 4;
					} else {
						low_word = 0;
					}

					high_word = trg_no & 0xFFFF;
				}

				// third DATA HEADER WORD contains TRIGGER number[47:32]
				if(link_head_counter == 3) {
					low_word  = (trg_no >> 16) & 0xFFFF;
					high_word = 0;
				}

				// fourth DATA HEADER WORD contains STATUS BITS
				if(link_head_counter == 4) {
					if(ilink == 0)
						low_word = DEF_STATUS;
					else if(ilink == 1)
						low_word = DEF_STATUS;
					//  if (LINK_NO==1) low_word = 0x0581;
					//  else if (LINK_NO==2) {
					//    if (payload_size>0)  low_word = 0x0101;
					//    else                 low_word = 0x0100;
					//  }
					//}
					else if(ilink == 2)
						low_word = 0x0508;
					else if(ilink == 5)
						low_word = 0xFFFF;
					else
						low_word = 0x0501;
					msb_word = 0x0;
				}

				// report errors
				if(lsb_word != low_word) {
					if(DEBUG)
						cout << " ==> link " << ilink << " has error for LSB-data of header word " << link_head_counter << ":  expected 0x" << hex << low_word << " but found 0x" << lsb_word
						     << " for event " << dec << evtcount - 1 << hex << " at line 0x" << hex << setfill('0') << setw(8) << hexcount << dec << endl;
					if(ilink != 5)
						fout << " ==> link " << ilink << " has error for LSB-data of header word " << link_head_counter << ":  expected 0x" << hex << low_word << " but found 0x" << lsb_word
						     << " for event " << dec << evtcount - 1 << hex << " at line 0x" << hex << setfill('0') << setw(8) << hexcount << dec << endl;
				}

				if(msb_word != high_word) {
					if(DEBUG)
						cout << " ==> link " << ilink << " has error for MSB-data of header word " << link_head_counter << ":  expected 0x" << hex << high_word << " but found 0x" << msb_word
						     << " for event " << dec << evtcount - 1 << hex << " at line 0x" << hex << setfill('0') << setw(8) << hexcount << dec << endl;
					if(ilink != 5)
						fout << " ==> link " << ilink << " has error for MSB-data of header word " << link_head_counter << ":  expected 0x" << hex << high_word << " but found 0x" << msb_word
						     << " for event " << dec << evtcount - 1 << hex << " at line 0x" << hex << setfill('0') << setw(8) << hexcount << dec << endl;
				}
			}  // end of check_header

			// chech payload content
			if(check_payload) {
				// remember last pattern data with non null payload
				if(payload_counter == 0) {
					if(ilink == FIRST_LINK) {
						start_prev_data = prev_data;
						if(DEBUG)
							cout << "  ilink is " << ilink << " ==> set start_prev_data as 0x" << hex << start_prev_data << dec << endl;
					} else if(ilink < (FIRST_LINK + LINK_NO)) {
						prev_data = start_prev_data;
						if(DEBUG)
							cout << "  ilink is " << ilink << " ==> overwrite prev_data as 0x" << hex << prev_data << dec << endl;
					}
				}

				payload_counter++;

				if(DEBUG)
					cout << "  payload_counter = " << payload_counter << " has data_int = 0x" << hex << data_int[0] << dec << endl;

				if(prev_data == -1) {  // this is the very first payload word (expected to be zero)
					prev_data = data_int[0];
					if(DEBUG || prev_data != 0)
						cout << "   at evtcount " << evtcount << ": UNUSUAL starting counter of 0x" << hex << data_int[0] << dec << endl;
				} else {  // prev_data has been set to previous payload counter
					if(DEBUG)
						cout << "  previous prev_data at evtcount " << evtcount << ": data_int = 0x" << hex << data_int[0] << dec << " and prev_data = 0x" << hex << prev_data << dec << endl;
					if(data_int[0] != prev_data + 1) {
						is_payload_error = 1;
						prev_data        = prev_data + 1;
					} else
						prev_data = data_int[0];
				}

				// report payload_error BEFORE updating "prev_data"!!
				if(is_payload_error) {
					is_payload_error = 0;
					if(DEBUG) {
						cout << "  bad payload word 0x" << hex << data_int[0] << dec << " at word count " << payload_counter << " of " << word_in_payload << " for event " << evtcount - 1 << " tag "
						     << evtcount + evt_offset - 1 << "(0x" << hex << evtcount + evt_offset - 1 << dec << "): expected 0x" << hex << (prev_data) << dec << " at line 0x" << hex << setfill('0')
						     << setw(8) << hexcount << dec << endl;
					}

					fout << "  bad payload word 0x" << hex << data_int[0] << dec << " at word count " << payload_counter << " of " << word_in_payload << " for event " << evtcount - 1 << " tag "
					     << evtcount + evt_offset - 1 << "(0x" << hex << evtcount + evt_offset - 1 << dec << "): expected 0x" << hex << (prev_data) << dec << " at line 0x" << hex << setfill('0')
					     << setw(8) << hexcount << dec << endl;
				}

			}  // end (check_payload)

			// we finished header checks: enable payload checks on the next word for links
			// with payload
			if(link_head_counter == 4) {
				link_head_counter = 0;
				check_header      = 0;
				check_payload     = 1;
			}

			// we finished payload checks: enable header checks on next word
			if(check_payload == 1 && payload_counter == payload_size) {
				payload_counter = 0;
				check_payload   = 0;
				check_header    = 1;

				// cout<<endl<<" Exiting check_paylod loop for ilink="<<ilink;

				ilink++;
				// these are links without payloads
				if(ilink >= LINK_NO)
					payload_size = 0;

				// cout <<endl<<" ...next payload_size expected to be
				// "<<payload_size<<endl;
			}

			// check on number of 32-bit words to decide if we reached end of event
			if(word_counter == evt_size) {
				if(DEBUG) {
					cout << " => end of event " << evtcount;
					cout << " with data_int[0] = 0x" << hex << data_int[0];
					cout << " at hexcount = 0x" << hex << hexcount << dec << endl;
				}
				word_counter = 0;
				evt_start    = 1;
				last_data    = data_int[0];
			}

			//      wordcount++;
		}  // end of while (!fin.eof()) {

		cout << endl;
		cout << "Processed " << evtcount << " events out of " << ievtno << " in file " << file_name << endl;

	}  // end of if (!fin.is_open()) {

	fout.close();
	fin.close();

	return 0;
}
