#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sstream>
#include <unistd.h>
#include <math.h>
#include <fstream>
#include <vector>
#include <signal.h>

#include <sqlite_modern_cpp.h>

using namespace sqlite;
using namespace std;


struct NetworkObj
{
	string SSID, ISP, BSSID;
	double download, upload, gpslat, gpslong;
	int passOp, loginOp; // 0 = No 1 = Yes because there's no point in doing bool conversion from Sql databases that I know of.
};

struct ScanObj
{
	string SSID, BSSID, security;
	int RSSI;
};

void ClearScreen()
{
	cout << "\x1B[2J\x1B[H"; //Ugly way to clear the screen for UNIX systems
}

void ctrlc_handler(int s)
{
	printf("\nCaught signal %d\nExiting...\n",s);
	exit(1);
}

void checkDependencies() //TODO: Makefile?
{

}

string exec(const char* cmd)
{
	array<char, 128> buffer;
	string result;
	shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (!feof(pipe.get())) {
		if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
			result += buffer.data();
	}
	return result;
}

void trim(std::string& s)
{
	 size_t p = s.find_first_not_of(" \t");
	 s.erase(0, p);

	 p = s.find_last_not_of(" \t");
	if (std::string::npos != p)
		s.erase(p+1);
}

class SpeedTest
{
private:
	bool testFinished;
	//mutex m_mutex;

	int parseTest(string & input, NetworkObj & output)
	//WARNING: ONLY PARSES speedtest-cli OUTPUT. Could use some cleanup in further iterations with optional adaptability for other interfaces
	{
		trim(input);
		stringstream ss(input);
		string temp, temp2;
		while(getline(ss, temp))
		{
			if(temp.compare("Download:")==1) //Note: I know this isn't how this should work but it does? Feedback is welcome. Should be checking '==0'
			{
				std::istringstream iss(temp);
				iss >> temp2 >> output.download;
			}
			else if(temp.compare("Upload:") ==1)
			{
				istringstream iss(temp);
				iss >> temp2 >> output.upload;
			}
			else if(temp.compare("Testing from")==1)
			{
				istringstream iss(temp);
				iss >> temp2  >> temp2 >> output.ISP;
			}
		}
		if(output.download < 0 || output.download > 1500)
		{
			cerr << "Did not find download speed." << endl;
			return 1;
		}
		else if(output.upload <= 0.0)
		{
			cerr << "Did not find upload speed." << endl;
			return 1;
		}
		else if(output.ISP.empty())
		{
			cerr << "Did not find ISP." << endl;
			return 1;
		}
		return 0;
	}

public:
	SpeedTest()
	{
		testFinished = false;
	}

	int runTest(NetworkObj const & output, const char* choice)
	{
		NetworkObj & outputCopy = const_cast<NetworkObj &>(output);

		string unparsedOutput = exec(choice);
		if(unparsedOutput.find("Cannot retrieve speedtest configuration") ==42)
		{
			cerr << "Test could not connect." << endl;
			return 1;
		}

		if(parseTest(unparsedOutput, outputCopy) != 0)
		{
			return 1;
		}

		/*********************************/
		//TODO: Password and login tests are not yet implemented. Currently manually assumes no password or login.
		outputCopy.passOp = 0;
		outputCopy.loginOp = 0;
		/*********************************/


		testFinished = true;
		return 0;
	}

	void loading() //Currently not used, but will be implemented in a separate thread in the future.
	{
		const int trigger = 500; // ms
		const int numDots = 4;
		const char prompt[] = "Running";
			//m_mutex.lock();
		while (!testFinished)
		{
				// Return and clear with spaces, then return and print prompt.
			printf("\r%*s\r%s", sizeof(prompt) - 1 + numDots, "", prompt);
			fflush(stdout);

				// Print numDots number of dots, one every trigger milliseconds.
			for (int i = 0; i < numDots; i++)
			{
				usleep(trigger * 1000);
				fputc('.', stdout);
				fflush(stdout);
			}
		}
	}

};

int getGPS(NetworkObj &theNetwork)
{
	#ifdef __APPLE__
		#ifdef __MACH__
			string output = exec("corelocationcli");
			istringstream iss(output);
			//cout << "GPS: " << output << endl; //VERBOSE: Print GPS data
			if(iss >> theNetwork.gpslat >> theNetwork.gpslong)
			{
				return 0;
			}
			else
			{
				cerr << "Problem grabbing Mac GPS data.";
				return 1;
			}
		#endif

	#elif __linux__ || __unix__
		cerr << "GPS NOT IMPLEMENTED YET" << endl;
		theNetwork.gpslat = 0;
		theNetwork.gpslat = 0;
		return 1;
	#else
		return 1;
	#endif
}


int runSpeedTest(NetworkObj& output)
{
	SpeedTest testObject;
	char const* testType = "speedtest-cli"; //Change to change speedtest used. Only supports speedtest-cli right now due to SpeedTest class parseTest() function


	if(testObject.runTest(output, testType)!=0)
	{
		cerr << "Test failed in runTest()"<<endl; //ERROR
		return 1;
	}

	cout << "Speed test finished for " << output.SSID << "." << endl; //VERBOSE
	cout << "DOWNLOAD: " <<output.download << endl; //VERBOSE: Download speed for specified network
	cout << "UPLOAD: " <<output.upload << endl; //VERBOSE: Upload speed for specified network
	cout << endl;

	return 0;
}



int scanNetworks(vector<ScanObj>& netScan)
{

	#ifdef __APPLE__
		#ifdef __MACH__
			string wifiInfo = exec("airport -s");

			trim(wifiInfo);
			if(wifiInfo.compare("No networks found")==1)
			{
				cerr << "No wifi networks in range." << endl;
				return 1;
			}
			else
			{
				//cout << wifiInfo; //VERBOSE: Prints the current list of wifi networks
			}

			stringstream ss(wifiInfo);
			string temp, temp2;
			getline(ss, temp);
			string::size_type sz;
			while(getline(ss, temp))
			{
				ScanObj row;
				istringstream iss(temp);
				int i = 0;
				while(iss>>temp2)
				{
					if(i==0)
					{
						row.SSID=temp2;
					}
					else if(i==1&&!(temp2.length()==17))
					{
						row.SSID = row.SSID+" "+temp2;
						i--;
					}
					else if(i==1)
					{
						row.BSSID = temp2;
					}
					else if(i==2)
					{
						row.RSSI = stoi(temp2, &sz);
					}
					else if(i==6)
					{
						row.security = temp2; //TODO: Parse security string
					}
					i++;
				}
				netScan.push_back(row);
			}
			return 0;
		#endif


	#elif __linux__ || __unix__  //TODO: Parse output
		string wifiInfo = exec("sudo iwlist wlan0 scan | grep \"Address\\|ESSID\\|Encryption key\\|Signal level\\|WPA\"");
		//cout << wifiInfo << endl; //DEBUG: Prints raw output

		stringstream ss(wifiInfo);
		string temp, temp2, securityString;
		int i = 0;
		int b = 0;
		bool security;
		string::size_type sz;
		ScanObj * row = new ScanObj;

		while(getline(ss, temp))
		{
			trim(temp);
			if(i>2&&(temp.substr(0,4)).compare("Cell")==0) //On the next cell
			{
				//Reset variables for next block
				i=0;
				security = false;
				securityString = "";

				//////////////////////////////////////
				//DEBUG: Prints the ScanObj right before it's pushed into the vector
				//cout << row->SSID << endl;
				//cout << row->BSSID << endl;
				//cout << row->RSSI << endl;
				//cout << row->security << endl;
				//cout << endl;
				//////////////////////////////////////

				netScan.push_back(*row);
				row = new ScanObj;
			}

			if(i==0)
			{
				row->BSSID = temp.substr(19,(temp.length()-1));
			}
			else if(i==1)
			{
				//cout << temp << "." << endl;
				//cout << temp.length() << endl;
				//cout << (temp.substr(28,(temp.length()-3))) << endl;
				row->RSSI = stoi(temp.substr(28,(temp.length()-3)),&sz);
			}
			else if(i==2)
			{
				securityString = temp.substr(15,temp.length()-1);
				//cout << "securityString =" << securityString.compare("on") << endl;
				if(securityString.compare("on")==0)
				{
					security = true;
				}
				else if(securityString.compare("off")==0)
				{
					security = false;
					row->security = "NONE";
				}
				else
				{
					cerr << "Did not grab encryption string in scan." << endl;
					return 1;
				}
			}
			else if(i==3)
			{
				row->SSID = temp.substr(7,(temp.length()-8));
			}
			else if(security)
			{
				if(i>4)
				{
					row->security += " ";
				}
				if(temp.find("WPA2") != string::npos)
				{
					row->security += "WPA";
				}
				else if(temp.find("WPA") != string::npos)
				{
					row->security += "WPA2";
				}
				else if(temp.find("WEP") != string::npos)
				{
					row->security += "WEP";
				}
				else
				{
					cerr << "Invalid security parse." << endl;
					cerr << "Current line data: " << temp << endl;
					return 1;
				}
			}
			else
			{
				cerr << "Too much data" << endl;
				cerr << temp << endl;
				//return 1;
			}
			i++;
		}
		return 0;

	#else
		cerr << "Unknown compiler";
		return 0;

	#endif
}

int searchNetworkDB(ScanObj& scanIn, NetworkObj& network)
//-1 is error, 0 is in DB, 1 is not in DB
//modifys network to return all values into the object
{
	try
	{
		database db("./Databases/liteDBTest.db");

		db <<
         "create table if not exists speed ("
         "   BSSID string primary key not null,"
         "   SSID string NOT NULL,"
         "   isp text,"
         "   download double,"
				 "   upload double,"
				 "   password bool,"
				 "   weblogin bool,"
				 "   gpslat decimal(9,6),"
				 "   gpslong decimal(9,6)"
         ");";

		db << "SELECT BSSID, SSID, isp, download, upload, password, weblogin, gpslat, gpslong FROM speed WHERE BSSID = ?"
			 << scanIn.BSSID >> tie(network.BSSID, network.SSID, network.ISP, network.download, network.upload, network.passOp, network.loginOp, network.gpslat, network.gpslong);
	}
	catch(sqlite_exception& e)
	{
		if(e.get_code() == 101)
		{
			//cout << scanIn.SSID << " is not in the database." << endl; //VERBOSE
			return 1;
		}
		cerr << "Search error: "<< e.what() << endl;
		return -1;
	}

	if(scanIn.BSSID.compare(network.BSSID) == 0)
	{
		//cout << "BSSIDs are the same." << endl; //VERBOSE
		return 0;
	}
	cerr << "Uncaught error." << endl;
	return -1;
}

const std::string& searchPasswordDB(std::vector<std::string>& network) //TODO
{

}

int addToNetworkDB(NetworkObj& network)
{
	//Create speed table and database if they do not exist
	try
	{
		database db("./Databases/liteDBTest.db");
		db <<
         "create table if not exists speed ("
         "   BSSID string primary key not null,"
         "   SSID string NOT NULL,"
         "   isp text,"
         "   download double,"
				 "   upload double,"
				 "   password bool,"
				 "   weblogin bool,"
				 "   gpslat decimal(9,6),"
				 "   gpslong decimal(9,6)"
         ");";


		db << "insert into speed values(?,?,?,?,?,?,?,?,?);"
			 << network.BSSID
			 << network.SSID
			 << network.ISP
			 << network.download
			 << network.upload
			 << network.passOp
			 << network.loginOp
			 << network.gpslat
			 << network.gpslong;
	}
	catch(exception e)
	{
		cerr<<"NOT INSERTED"<<endl;
		cerr << "ERROR: " << e.what() << endl;
		return 1;
	}
	return 0;
}

int connectToNetwork(ScanObj& network) //1 is error, 0 is success
{
	#ifdef __APPLE__
		#ifdef __MACH__
			std::string interface = "en0";
			if(network.security.compare("NONE") == 0) //
			{
				string toRun = "networksetup -setairportnetwork " + interface + " " + network.SSID;

				int didConnect= (exec(toRun.c_str())).compare("");
				int test = 0;
				while(exec("networksetup -getairportnetwork en0").find("You are not associated with an AirPort network.")==46) //Run a loop until the wifi card says the connection is stable.
				{
					if(didConnect != 0)
					{
						return 1;
					}
					test++;
				}

				if(didConnect== 0) //connects to network
				{
					cout << "Connected to " << network.SSID << endl; //VERBOSE
					sleep(1); //Wait for a second on connection to network to make sure system fully connects
				}
				else
				{
					cerr<<"Could not connect to "<<network.SSID << endl;
					return 1;
				}
			}

			else //NOT IMPLEMENTED -- search password DB
			{
				//password = searchPasswordDB(network);
			}
			return 0;
		#endif

	#elif __linux__ || __unix__ // all unices not caught above
		string interface = "wlan0";
		if(network.security.compare("NONE"))
		{
			string toRun = "wpa_cli -i "+interface+" select_network $(wpa_cli -i "+interface+" list_networks | grep "+network.SSID+" | cut -f 1)";
			exec(toRun.c_str()); //TODO set to variable and check output for errors
			//https://www.systutorials.com/docs/linux/man/8-wpa_cli/
		}

		else //search password DB
		{
			//password = searchPasswordDB(network);
		}
		return 0;
	#endif
	cerr << "OS detection in network connect is acting funky" << endl;
	return 1;
}


int unsecuredNetworkOption(ScanObj & networkRun)
{
	if(connectToNetwork(networkRun) == 1)
	{
		cerr << "Failed connecting." << endl <<endl;
		return 1;
	}
	NetworkObj completedTest;

	completedTest.SSID = networkRun.SSID;
	completedTest.BSSID = networkRun.BSSID;

	if(runSpeedTest(completedTest) !=0) //Speedtest fails
	{
		cerr << "Speedtest failed back to unsecuredOption()." << endl << endl;
		return 1;
	}
	else //speedtest is successful
	{
		if(getGPS(completedTest)==1)
		{
			completedTest.gpslat = 0;
			completedTest.gpslong = 0;
		}
		addToNetworkDB(completedTest);
	}
	return 0;
}

int main(int argc,char** argv)
//This will all be cleaned up in later iterations
{
	string output, location;
	ofstream speedFile;
	vector<ScanObj> wifiList;
	vector<string> testNetwork;
	vector<ScanObj> wifiTemp; //So we don't scan the same network in the same location
	vector<ScanObj> untestedNetworks;
	vector<NetworkObj> testedNetworks;
	bool scan = true;
	int check;

	//freopen("outputlog.txt", "w", stdout); //Moves output stream to outputlog.txt
	freopen("./logs/errorlog.txt", "w", stderr); //Moves error stream to errorlog.txt

////////////////////////////////
//Below handles catching Ctrl-C
	struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = ctrlc_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);
////////////////////////////////



	//ClearScreen();

	//exec("networksetup -setairportpower en0 on"); //DEBUG: MAC ONLY

	cout << "Welcome to MrCheese123's network logger!" << endl
			 << "A couple things are a work in progress, but everything is currently working on most versions of MacOS and Raspbian." <<endl
			 << "To end the logger, press Ctrl-C (will be changed to ESC in the future)"
			 << endl;

	while(true) //Main loop
	{
		if(scanNetworks(wifiList)==1)
		{
			scan = false;
			cerr<<"No networks in range."<<endl;
		}
		else
		{
			scan = true;
		}

		if(scan)
		{
			for (auto i: wifiList)
			{
				NetworkObj scanOutput;
				check = searchNetworkDB(i, scanOutput);
				if(check == 0)
				{
					testedNetworks.push_back(scanOutput);
					cout << scanOutput.SSID << " is in the database." << endl; //VERBOSE
				}
				else if (check == 1)
				{
					if(wifiTemp.empty()!=true)
					{
						bool wasLast = false;
						for(auto b:wifiTemp)
						{
							if(b.BSSID==i.BSSID)
							{
								wasLast=true;
								break;
							}
						}
						if(wasLast)
						{
							continue;
						}
					}
					untestedNetworks.push_back(i);
				}
				else
				{
					cerr << "SQL NOT FUNCTIONING PROPERLY. EXITING." << endl;
					return 1;
				}
			}
			wifiTemp=untestedNetworks; //reset temp to the current list.

			for(auto i: untestedNetworks)
			{
				if(i.security.compare("NONE") == 0)
				{
					cout << "Attempting test on " << i.SSID << endl; //VERBOSE
					unsecuredNetworkOption(i);
				}
				else
				{
					cerr <<"Networks with passwords are currently unsupported. " << endl; //VERBOSE
					cerr << i.SSID << " has a security type of: " << i.security << endl;  //VERBOSE
					cerr << endl; //VERBOSE
				}
			}
		}
		cout << "Sleeping..." << endl; //VERBOSE
		sleep(15); //Adjust to reduce or increase wait times between scans
		output = "";
		location = "";
		wifiList.clear();
		testNetwork.clear();
		untestedNetworks.clear();
		testedNetworks.clear();
		wifiTemp.clear();


		scan = true;
	}
}
