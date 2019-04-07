#include <string>
#include <sqlite_modern_cpp.h>

using namespace sqlite;
using namespace std;

int printSpeed()
{
  try
  {
    database db("./liteDBTest.db");

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
   cout << "       BSSID      |    SSID    |   ISP   |   Download   |   Upload   |   Password   |   Web Login   |   Latitude   |   Longitute" << endl;
   db << "select * from speed;"
        >> [&](string BSSID, string SSID, string isp, double download, double upload, bool pass, bool web, double gpslat, double gpslong)
        {
           cout << BSSID << " | " << SSID << " | " << isp << " | " << download << " | " << upload << " | " << pass << " | " << web << " | " << gpslat << " | " << gpslong << endl;
        };
  }
 	catch(sqlite_exception& e)
 	{
 		cerr << "Search error: "<< e.what() << endl;
 		return 1;
 	}
  return 0;
}


int main()
{
  printSpeed();
}
