//create table speed(SSID varchar(30) NOT NULL, isp text, download double, upload double, password bool, weblogin bool, PRIMARY KEY(SSID));
//g++ -std=c++14 main.cpp -o speedt -l sqlite3 // -pthread

/*
_______ ____  _____   ____
|__   __/ __ \|  __ \ / __ \
	| | | |  | | |  | | |  | |
	| | | |  | | |  | | |  | |
	| | | |__| | |__| | |__| |
	|_|  \____/|_____/ \____/

	SEV 1:
	Main loop does not loop properly. For loop for unsecuredOption() hanging?  --Done?

	SEV 2:
	Password DB
	Multithread connection status to end speedtest on network disconnect.
	Add --verbose option

	Implement GPS database push - Done?
	Test GPS database push >> Make sure iss parsing in getGPS() is correct

	SEV 3:
	Run network scan before next network in line for test? Speeds up testing. No need to reparse, just search scan for BSSID. Already checks the database.
	Multithread this? EG run the scan in a separate thread and search the object for the BSSID.

	SEV 4:
	Create header file
	Separate functions into multiple files

	FEATURE:
	Measure RSSI in separate thread to test signal strength
	Implement GPS logging on Pi
	Wifi browser login
*/

/*
DEPENDENCIES:

All systems:
https://github.com/sivel/speedtest-cli
SOCI librarys. Needs to be installed to system. Automate install?
Sqlite3
https://github.com/sindresorhus/fast-cli

Mac OS only:
https://github.com/fulldecent/corelocationcli

Raspberry Pi only:
GPS utility:

*/

//NOTE: How to implement auto-login on public wifi
//https://github.com/mutantmonkey/openwifi
/*
If all you need to do is open a browser and accept terms + conditions .... then maybe you can simply open a chrome debug window and make note of what POST values get sent to their website once you accept, and what URL those values are sent to.

Then just write a local HTML file with an iframe inside it. The iframe would point to any internet site, in order to cause it to be populated with the "please accept our conditions" screen.

Then write a form on your main window that contains hidden fields that correspond to all the POST values that need to be sent, from earlier. Give the form an "action" of the same URL from above as well. Give the form an ID so you can access it from javascript.

Then write some short simple javascript code that basically waits a few seconds, and then submits the form.

Of course you'll then need to have raspbian auto-login to your account, and spawn a web browser with your custom HTML file as a homepage.

If you're lucky, that should do the trick.
*/
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
//#include <thread>
#include <term.h>
#include <math.h>
#include <fstream>
#include <vector>

#include <sqlite_modern_cpp.h>

//#define GetCurrentDir getcwd

using namespace sqlite;
using namespace std;


struct NetworkObj
{
	string SSID, ISP, BSSID;
	double download, upload, gpslat, gpslong;
	int passOp, loginOp; // 0 = No 1 = Yes because I don't want to work with bool conversion right now.
	//string password; //Encrypt?
};

struct ScanObj
{
	string SSID, BSSID, security;
	int RSSI;
};

//template<>//TODO: password type conversion after passworddb implementation
/*struct type_conversion<NetworkObj>
{
	typedef values base_type;

	static void from_base(values const & v, indicator, NetworkObj& sped)
	{
		sped.SSID = v.get<string>("SSID");
		sped.ISP = v.get<string>("isp");
		sped.download = v.get<double>("download");
		sped.upload = v.get<double>("upload");
		sped.passOp = v.get<int>("password");
		sped.loginOp = v.get<int>("weblogin");
	}

	static void to_base(const NetworkObj& sped, values& v, indicator& ind)
	{
		v.set("SSID", sped.SSID);
		v.set("ISP", sped.ISP);
		v.set("download", sped.download);
		v.set("upload", sped.upload);
		v.set("password", sped.passOp);
		v.set("weblogin", sped.loginOp);
		ind = i_ok;
	}
};*/

/*static int callback(void *NotUsed, int argc, char **argv, char **azColName) //Dissect and implement in searchNetworkDB()
{
   int i;
	 cout << "SUPRISE" << endl;
   for(i = 0; i<argc; i++)
	 {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}*/

void checkDependencies() //TODO: Makefile?
{

}

void ctrlc_handler(int s)
{
	printf("\nCaught signal %d\nExiting...\n",s);
	exit(1);
}

void trim(std::string& s) //Decrepatated? Now in SpeedObj.
{
	 size_t p = s.find_first_not_of(" \t");
	 s.erase(0, p);

	 p = s.find_last_not_of(" \t");
	if (std::string::npos != p)
		s.erase(p+1);
}


void ClearScreen()
{
	cout << "\x1B[2J\x1B[H"; //Ugly way to clear the screen for UNIX systems
}

std::string exec(const char* cmd)
{
	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (!feof(pipe.get())) {
		if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
			result += buffer.data();
	}
	return result;
}

/*void askPassword(std::string& password)//TODO: Pull wifi password from password database
{
	char inTemp;
	while(true)
	{
		cout << "Password? (y/n): ";
		cin >> inTemp;
		if(inTemp=='y')
		{
			cout << "Enter password: ";
			cin >> password;
			break;
		}
		else if(inTemp=='n')
		{
			password = "NONE";
			break;
		}
		else
		{
			cout << "Incorrect input." << std::endl;
			inTemp = ' ';
			continue;
		}
	}
}*/


/*
 public:
	SpeedObj(string& cSSID, string& cISP, double& cDownload, double& cUpload, int& cPassOp, int& cLoginOp, string& cPassword = NULL)
	 :SSID(cSSID), ISP(cISP), download(cDownload), upload(cUpload), passOp(cPassOp), loginOp(cLoginOp), password(cPassword)
	 {}

	string getSSID() const
	{ return SSID; }

	string getISP() const
	{ return ISP; }

	double getDownload() const
	{ return download; }

	double getUpload() const
	{ return upload; }

	bool hasPass() const
	{ return passOp; }

	bool hasLogin() const
	{ return loginOp; }

};*/

class SpeedTest
{
	bool testFinished;
	//mutex m_mutex;

	int parseTest(string & input, NetworkObj & output) //WARNING: ONLY PARSES speedtest-cli OUTPUT. Decrepatated parse above can parse fast-cli
	{
		trim(input); //Should I move this inside the class for data protection? Do I care?
		stringstream ss(input);
		//cout << "Output: " << output << endl; //DEBUG: Prints pre-parsed speedtest
		string temp, temp2;
		while(getline(ss, temp))
		{
			if(temp.compare("Download:")==1)
			//WARNING: So this shouldn't be working? should return 0...
			{
				std::istringstream iss(temp);
				//cout << "DL: "<< temp << endl; //DEBUG
				iss >> temp2 >> output.download;
				//cout << "Found download : " << output.download << endl; //DEBUG
			}
			else if(temp.compare("Upload:") ==1)
			{
				istringstream iss(temp);
				//cout << "UL: "<< temp << endl; //DEBUG
				iss >> temp2 >> output.upload;
			}
			else if(temp.compare("Testing from")==1)
			{
				istringstream iss(temp);
				iss >> temp2  >> temp2 >> output.ISP;
			}
		}
		//cout << "DOWNLOAD: " <<output.download << endl; //VERBOSE: Download speed for specified network
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
		//size_t testingSubstr = unparsedOutput.find("Cannot retrieve speedtest configuration"); //ERROR CHECK
		//cout << "SUBSTRING TEST: " << testingSubstr << endl;//Retrieving speedtest.net configuration... Cannot retrieve speedtest configuration
		if(unparsedOutput.find("Cannot retrieve speedtest configuration") ==42)
		{
			cerr << "Test could not connect." << endl << endl;
			return 1;
		}

		if(parseTest(unparsedOutput, outputCopy) != 0)
		{
			return 1;
		}

		/*********************************/
		//WARNING: CHECK IF PASSWORD? CHECK WEBLOGIN? Currently manually assumes no password.
		//askPassword(password);
		//askWebLogin(webLogin);
		outputCopy.passOp = 0;
		outputCopy.loginOp = 0;
		/*********************************/


		testFinished = true;
		return 0;
	}

	/*void debugRunTest(std::string const &output, const char* choice)
	{
			//lock_guard<std::mutex> guard(m_mutex);
		std::string & outputCopy = const_cast<std::string &>(output);
			//cout << "about to run - "<<choice;
		outputCopy = exec(choice);


		//TODO: Put test failure check here instead of parseTextOutput()
		testFinished = true;
	}*/

	void loading()
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
			//cout << "GPS: " << output << endl; //DEBUG
			if(iss >> theNetwork.gpslat >> theNetwork.gpslong) //Change to try-catch?
			{
				return 0;
			}
			else
			{
				cerr << "Problem grabbing Mac GPS data.";
				//LOCATION MANAGER ERROR: The operation couldn’t be completed. (kCLErrorDomain error 0.)
				return 1;
			}
		#endif

		#elif __linux__ || __unix__ //WARNING: TODO: Grab Pi GPS data
			cerr << "GPS NOT IMPLEMENTED YET" << endl;
			theNetwork.gpslat = 0;
			theNetwork.gpslat = 0;
			return 1;
		#else
			return 1;
		#endif
}

/*
	//Spin up two threads literally only for the loading animation of the ellipse...
	std::thread speedThread(&SpeedTest::debugRunTest, &testObject, ref(output), testType);
	std::thread loadThread(&SpeedTest::loading, &testObject);
	if(speedThread.joinable())
	{
		speedThread.join();
	}
	if(loadThread.joinable())
	{
		loadThread.join();
	}

	//std::thread speedThread(&SpeedTest::runTest, &testObject, ref(output), testType);
	//if(speedThread.joinable())
	//{
	//	speedThread.join();
	//}
*/

int runSpeedTest(NetworkObj& output)
{
	SpeedTest testObject;
	char const* testType = "speedtest-cli"; //Change to change speedtest used. Only supports speedtest-cli right now due to SpeedTest parseTest class

	//Spin up runTest thread
	//void runTest(NetworkObj const & output, const char* choice)
	if(testObject.runTest(output, testType)!=0) //TODO: Does not catch speedtest fail
	{
		cerr << "Test failed in runTest()"<<endl;
		return 1;
	}



	cout << "Speed test finished for " << output.SSID << "." << endl << endl; //VERBOSE
	//cout << "DOWNLOAD: " <<output.download <<endl; //DEBUG

	return 0; //TODO: More error checking?
}



int scanNetworks(std::vector<ScanObj>& netScan) //TODO: fix for linux parsing
{

	#ifdef __APPLE__
		#ifdef __MACH__
			std::string wifiInfo = exec("airport -s");
			trim(wifiInfo);
			if(wifiInfo.compare("No networks found")==1)
			{
				std::cerr << "No wifi networks in range." << endl;
				return 1;
			}
			else
			{
				//cout << wifiInfo;
			}
			std::stringstream ss(wifiInfo);
			std::string temp, temp2;
			//TODO: parse quality and password needs
			getline(ss, temp);
			string::size_type sz;
			while(getline(ss, temp)) //TODO: CLEAN THIS UP
			{
				ScanObj row;
				std::istringstream iss(temp);
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


	#elif __linux__ || __unix__ // all unices not caught above
		wifiInfo = exec("sudo iwlist wlan0 scan | grep \"Address\|ESSID\|Encryption key\|Signal level\"");
		/*
		Cell 23 - Address: 06:18:0A:7F:37:16
			Quality=26/70  Signal level=-84 dBm
      Encryption key:on
      ESSID:"Specht Guest"
		*/
		stringstream ss(wifiInfo);
		string temp, temp2;
		int i = 0;
		while(getline(ss, temp))
		{
			ScanObj row;
			std::istringstream iss(temp);
			cout << "i = " << i << endl;

			while(iss>>temp2)
			{
				cout << temp2 << endl;
			}

			if(i==3)
			{
				netScan.push_back(row);
				i=0;
				continue;
			}
			i++;
		}

	#else
		cerr << "Unknown compiler";
		return 0;
	#endif

}

void debugNetworkScan() //TODO: DEBUG:
{

}

/*vector<char> toVector( const std::string& s ) //Do I need this?
{
  vector<char> v(s.size()+1);
  memcpy( &v.front(), s.c_str(), s.size() + 1 );
  return v;
}*/

int searchNetworkDB(ScanObj& scanIn, NetworkObj& network) //TODO: Test this!!!
//std::vector<std::vector<std::string> >& wifiList)
//TODO: CHANGE TO MORE UNIQUE IDENTIFIER FOR NETWORK, NOT SSID
//-1 is error, 0 is in DB, 1 is not in DB
//modifys network to return all values into the object
{
	try
	{
		database db("./Databases/liteDBTest.db");
		db << "SELECT BSSID, SSID, isp, download, upload, password, weblogin, gpslat, gpslong FROM speed WHERE BSSID = ?"
			 << scanIn.BSSID >> tie(network.BSSID, network.SSID, network.ISP, network.download, network.upload, network.passOp, network.loginOp, network.gpslat, network.gpslong);
	}
	catch(sqlite_exception& e)
	{
		if(e.get_code() == 101)
		{
			cout << scanIn.SSID << " is not in the database." << endl; //VERBOSE
			return 1;
		}
		cerr << "Search error: "<< e.what() << endl;
		return -1;
	}

	if(scanIn.BSSID.compare(network.BSSID) == 0)
	{
		//cout << "BSSIDs are the same. "; //DEBUG
		return 0;
	}
	cerr << "Uncaught error." << endl;
	return -1;

/********************DEBUG PRINT SQL SERVER***********************/
//create table speed(SSID varchar(30) NOT NULL, isp text, download double, upload double, password bool, weblogin bool, PRIMARY KEY(SSID));
/*	rowset<row> rs = (sql.prepare << "select SSID, isp, download, upload, password, weblogin from speed");

	// iteration through the resultset:
	for (rowset<row>::const_iterator it = rs.begin(); it != rs.end(); ++it)
	{
	    row const& row = *it;

			cout << "SSID|ISP|Download|Upload|Password|Weblogin" <<endl;
	    // dynamic data extraction from each row:
	    cout << row.get<string>(0) << '|'
	         << "ISP: " << row.get<string>(1) << '|'
	         << "Download: " << row.get<double>(2) << '|'
					 << "Upload: " << row.get<double>(3) << '|'
					 << "Password: " << row.get<int>(4) << '|'
					 << "Weblogin: " << row.get<int>(5) << endl;
	}*/
	/*****************************************************************/
//	cerr << "HOW DID YOU BYPASS SQL CHECK?";
//	return -1;
}

const std::string& searchPasswordDB(std::vector<std::string>& network) //TODO
{

}

int addToNetworkDB(NetworkObj& network) //TODO: Test new SQL library calls
{

	//Create speed table if it does not exist
	//TODO: Convert BSSID to BIGINT http://www.onurguzel.com/storing-mac-address-in-a-mysql-database/
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

int connectToNetwork(ScanObj& network) //1 is error, 0 is success TODO: Test on Pi, Test on macOS
{
	#ifdef __APPLE__
		#ifdef __MACH__ //TODO: Find new os target for macOS
			std::string interface = "en0"; //TODO: Detect interface.
			if(network.security.compare("NONE") == 0) //
			{
				std::string toRun = "networksetup -setairportnetwork " + interface + " " + network.SSID;

				int didConnect= (exec(toRun.c_str())).compare("");
				int test = 0;
				//cout << exec("networksetup -getairportnetwork en0").substr(0,46);
				while(exec("networksetup -getairportnetwork en0").find("You are not associated with an AirPort network.")==46) //Run a loop until the wifi card says the connection is stable.
				{
					//cout<<test << " "; //VERBOSE
					if(didConnect != 0)
					{
						return 1;
					}
					test++;
				}

				if(didConnect== 0) //connects to network
				{
					cout << "Connected to " << network.SSID << endl; //VERBOSE
					sleep(2);
				}
				else
				{
					cerr<<"Could not connect to "<<network.SSID << endl;
					return 1;
				}
			}

			else //search password DB
			{
				//password = searchPasswordDB(network);
			}
			return 0;
		#endif
	#elif __linux__ || __unix__ // all unices not caught above
		//NEED THE FOLLOWING: https://www.systutorials.com/docs/linux/man/8-wpa_cli/
		string interface = "wlan0"; //TODO: Detect interface.
		if(network.security.compare("NONE"))
		{
			string toRun = "wpa_cli -i "+interface+" select_network $(wpa_cli -i "+interface+" list_networks | grep "+network[0]+" | cut -f 1)";
			exec(toRun); //https://www.systutorials.com/docs/linux/man/8-wpa_cli/
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

	if(runSpeedTest(completedTest) !=0) //speedtest fails
	{
		std::cerr << "Speedtest failed back to unsecuredOption()." << endl << endl;
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


		/*DEBUG: Only runs on linux*/

		/***************************/
		//int speeds[3] =
		// run a process and create a streambuf that reads its stdout and stderr
		/*redi::ipstream proc("speed-test", redi::pstreams::pstdout | redi::pstreams::pstderr);
		string line;
		// read child's stdout
		while (getline(proc.out(), line))
		{
			cout << "stdout: " << line << '\n';
		}

		// read child's stderr
		while (getline(proc.err(), line))
		cout << "stderr: " << line << '\n';*/

		//std::cout<< "Input location: "; //TODO: move to function
		//getline(std::cin, location);
	}
	return 0;
}

int main()
{
	ClearScreen();

	std::string output, location;
	std::ofstream speedFile;
	std::vector< ScanObj > wifiList; //better with multidimensional arrays?
	std::vector<std::string> testNetwork;
	std::vector< ScanObj > wifiTemp; //So we don't scan the same network in the same location
	vector<ScanObj> untestedNetworks;
	vector<NetworkObj> testedNetworks;
	bool scan = true;

	////////////////////////////////
	//Below handles catching Ctrl-C
		struct sigaction sigIntHandler;

	  sigIntHandler.sa_handler = ctrlc_handler;
	  sigemptyset(&sigIntHandler.sa_mask);
	  sigIntHandler.sa_flags = 0;

	  sigaction(SIGINT, &sigIntHandler, NULL);
	////////////////////////////////

	//checkDependencies(); //TODO: Check for package dependencies like NPM speedtest package

	//exec("networksetup -setairportpower en0 on"); //DEBUG: MAC ONLY -- Turns on WiFi card
	

	while(true)
	{
		if(scanNetworks(wifiList)==1)
		{	scan = false; cerr<<"No networks in range."<<endl; }
		else
		{ scan = true; }

		if(scan)
		{
			for (auto i: wifiList)
			{
				NetworkObj scanOutput;
				int check = searchNetworkDB(i, scanOutput);
				if(check == 0)
				{
					testedNetworks.push_back(scanOutput);
					//cout << scanOutput.SSID << " is in the database." << endl;
				}
				else if (check == 1)
				{
					if(wifiTemp.empty()!=true)//TODO: Fix this line. Goes through last wifi list so we don't do extra scans
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
					cerr << "SQL NOT FUNCTIONING PROPERLY. EXITTING.";
					return 1;
				}
			}
			wifiTemp=untestedNetworks; //reset temp to the current list.
			cout << endl;

			for(auto i: untestedNetworks) //TODO: Move to function
			{
				if(i.security.compare("NONE") == 0)
				{
					cout << "Attempting test on " << i.SSID << endl; //VERBOSE
					unsecuredNetworkOption(i);
				}
				else
				{
					cerr<<"Networks with passwords are currently unsupported. " << endl;
					cerr<< i.SSID << " has a security type of: " << i.security << endl;
					cerr << endl;
				}
			}
		}
		cout << "Sleeping..." << endl; //VERBOSE
		sleep(30); //Waits 15 seconds before repeating.
		ClearScreen();
		//cout << "Clearing variables and running again." << endl; //VERBOSE
		output = "";
		location = "";
		wifiList.clear();
		testNetwork.clear();
		untestedNetworks.clear();
		testedNetworks.clear();
		scan = true;
	}
	//So this sets up a text file to store everything. Should I keep it if the database dies as a backup?
	/*		std::ifstream ifsspeedFile("speedtest.txt");
			if(ifsspeedFile.fail())
			{
				speedFile.open("speedtest.txt", std::ios_base::app);
				speedFile << "Name | ISP | DL fast.com | DL speedtest.net | UL | Password | Login | GPS\n";
			}
			else
			{
				speedFile.open("speedtest.txt", std::ios_base::app);
				speedFile <<  "N/A" + output;
			}

			ifsspeedFile.close();*/


	//cout << "OS: " << OS << "\n"; //DEBUG

}
