/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:
	UIN:
	Date:
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>


using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	int buff_size = MAX_MESSAGE;
	bool channel = false;
	vector<FIFORequestChannel*> channels;

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p': p = atoi(optarg); break;
			case 't': t = atof(optarg); break;
			case 'e': e = atoi(optarg); break;
			case 'f': filename = optarg; break;
			case 'm': buff_size = atoi(optarg); break;
			case 'c': channel = true; break;
		}
	}
	
	pid_t child = fork();
    if(child<0){
        perror("fork");
        return 1;
    }
    if(child==0){
        execl("./server","./server",(char *)NULL);
        perror("execl ./server");
        return 1;
    }
	else { // parent
		
		sleep(1); // wait for server to run in child process
		FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(&cont_chan);

		if (channel) {
			MESSAGE_TYPE new_cmsg = NEWCHANNEL_MSG;
			cont_chan.cwrite(&new_cmsg, sizeof(MESSAGE_TYPE));
			char c[MAX_MESSAGE];
			cont_chan.cread(c, sizeof(c));
			string cname(c);
			FIFORequestChannel* new_chan = new FIFORequestChannel(cname, FIFORequestChannel::CLIENT_SIDE);
			channels.push_back(new_chan);
			cout << cname <<" created" << endl;
		}

		FIFORequestChannel* chan = channels.back(); // pointer to current channel

		char buf[MAX_MESSAGE];

		// single data point
		if (p != -1 && t >= 0.0 && e != -1) {
			double response;
			datamsg x(p, t, e);
			memcpy(buf, &x, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg));
			
			chan->cread(&response, sizeof(double));
			cout << "For person " << p << ", at time " << t
			     << ", the value of ecg " << e << " is " << response << endl;
		} 
		// first 1000 points
		else if (p != -1 && filename == "") {
			mkdir("received", 0777);
			ofstream ofs("received/x1.csv");
			if (!ofs) {
				cerr << "Error opening received/x1.csv\n";
				return 1;
			}
			for (int i = 0; i < 1000; i++) {
				t = i*0.004;
				datamsg x(p, t, 1);
				memcpy(buf, &x, sizeof(datamsg));
				chan->cwrite(buf, sizeof(datamsg));
				double ecg1;
				chan->cread(&ecg1, sizeof(double));

				datamsg y(p, t, 2);
				memcpy(buf, &y, sizeof(datamsg));
				chan->cwrite(buf, sizeof(datamsg));
				double ecg2;
				chan->cread(&ecg2, sizeof(double));

				ofs << t << "," << ecg1 << "," << ecg2 << endl;
			}
			ofs.close();
			cout << "1000 data points for person " << p
			     << " written to received/x1.csv" << endl;
		}
		// file request
		else if (filename != "") {
			filemsg fmsg(0, 0);
			int len = sizeof(filemsg) + (filename.size() + 1);
			char* buf2 = new char[len];
			memcpy(buf2, &fmsg, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), filename.c_str());
			chan->cwrite(buf2, len);

			int64_t f_size = 0;
			chan->cread(&f_size, sizeof(int64_t));
			cout << "File size: " << f_size << " bytes\n";

			filemsg* f_req = (filemsg*)buf2;
			f_req->offset = 0;

			char* buf3 = new char[buff_size];
			mkdir("received", 0777);
			ofstream ofs("received/" + filename, ios::binary);
			if (!ofs) {
				cerr << "Error opening received/" << filename << endl;
				delete[] buf2;
				delete[] buf3;
				return 1;
			}

			while (f_size > 0) {
				if (f_size < buff_size) {
					f_req->length = f_size;
				} else {
					f_req->length = buff_size;
				}
				chan->cwrite(buf2, len);
				chan->cread(buf3, f_req->length);
				ofs.write(buf3, f_req->length);

				f_size -= f_req->length;
				f_req->offset += f_req->length;
			}
			
			ofs.close();
			delete[] buf2;
			delete[] buf3;
			cout << "Request for " << filename << " complete" << endl;
		}

		// cleanup
		MESSAGE_TYPE m = QUIT_MSG;

		if (channel) {
			chan->cwrite(&m, sizeof(MESSAGE_TYPE));
			delete channels.back();
			channels.pop_back();
			chan = channels.back();
		}

		chan->cwrite(&m, sizeof(MESSAGE_TYPE));
		wait(NULL);
	}
}
