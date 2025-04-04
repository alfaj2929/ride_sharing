#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cstdlib>

using namespace std;
// Geohash precision (1-12)
const int GEOHASH_PRECISION = 6;

// Timeout for ride requests in seconds
const int REQUEST_TIMEOUT = 300; // 5 minutes

// Structure to represent a location with latitude and longitude
struct Location
{
    double latitude;
    double longitude;

    Location(double lat, double lng) : latitude(lat), longitude(lng) {}

    // Calculate distance between two locations (Haversine formula)
    double distanceTo(const Location &other) const
    {
        const double R = 6371.0; // Earth radius in km
        const double dLat = (other.latitude - latitude) * M_PI / 180.0;
        const double dLon = (other.longitude - longitude) * M_PI / 180.0;
        const double a = sin(dLat / 2) * sin(dLat / 2) +
                         cos(latitude * M_PI / 180.0) * cos(other.latitude * M_PI / 180.0) *
                             sin(dLon / 2) * sin(dLon / 2);
        const double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return R * c;
    }
};

// Driver class
class Driver
{
public:
    int id;
    Location location;
    chrono::system_clock::time_point lastActive;
    bool available;

    Driver(int id, double lat, double lng)
        : id(id), location(lat, lng), available(true)
    {
        lastActive = chrono::system_clock::now();
    }

    void updateLocation(double lat, double lng)
    {
        location = Location(lat, lng);
        lastActive = chrono::system_clock::now();
    }

    void setAvailable(bool status)
    {
        available = status;
        if (status)
        {
            lastActive = chrono::system_clock::now();
        }
    }

    string getLastActiveTime() const
    {
        auto now = chrono::system_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(now - lastActive).count();

        if (duration < 60)
        {
            return to_string(duration) + " seconds ago";
        }
        else if (duration < 3600)
        {
            return to_string(duration / 60) + " minutes ago";
        }
        else
        {
            return to_string(duration / 3600) + " hours ago";
        }
    }
};

// Passenger class
class Passenger
{
public:
    int id;
    
    Location location;
    chrono::system_clock::time_point requestTime;

    Passenger(int id, double lat, double lng)
        : id(id), location(lat, lng)
    {
        requestTime = chrono::system_clock::now();
    }

    bool isExpired() const
    {
        auto now = chrono::system_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(now - requestTime).count();
        return duration > REQUEST_TIMEOUT;
    }

    string getWaitTime() const
    {
        auto now = chrono::system_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(now - requestTime).count();

        if (duration < 60)
        {
            return to_string(duration) + " seconds";
        }
        else
        {
            return to_string(duration / 60) + " minutes " +
                   to_string(duration % 60) + " seconds";
        }
    }
};

// Trie node for geohash-based location storage
class TrieNode
{
public:
    unordered_map<char, shared_ptr<TrieNode>> children;
    vector<int> driverIds;

    void insertDriver(const string &geohash, int driverId, int index = 0)
    {
        if (index == geohash.length())
        {
            // Add driver to this node
            if (find(driverIds.begin(), driverIds.end(), driverId) == driverIds.end())
            {
                driverIds.push_back(driverId);
            }
            return;
        }

        char currentChar = geohash[index];
        if (children.find(currentChar) == children.end())
        {
            children[currentChar] = make_shared<TrieNode>();
        }
        children[currentChar]->insertDriver(geohash, driverId, index + 1);
    }

    void removeDriver(const string &geohash, int driverId, int index = 0)
    {
        if (index == geohash.length())
        {
            // Remove driver from this node
            auto it = find(driverIds.begin(), driverIds.end(), driverId);
            if (it != driverIds.end())
            {
                driverIds.erase(it);
            }
            return;
        }

        char currentChar = geohash[index];
        if (children.find(currentChar) != children.end())
        {
            children[currentChar]->removeDriver(geohash, driverId, index + 1);
        }
    }

    vector<int> findDriversWithPrefix(const string &prefix, int index = 0)
    {
        if (index == prefix.length())
        {
            // Collect all drivers in this subtree
            vector<int> result = driverIds;
            for (const auto &pair : children)
            {
                vector<int> childDrivers = pair.second->getAllDrivers();
                result.insert(result.end(), childDrivers.begin(), childDrivers.end());
            }
            return result;
        }

        char currentChar = prefix[index];
        if (children.find(currentChar) != children.end())
        {
            return children[currentChar]->findDriversWithPrefix(prefix, index + 1);
        }

        return {};
    }

    vector<int> getAllDrivers() const
    {
        vector<int> result = driverIds;
        for (const auto &pair : children)
        {
            vector<int> childDrivers = pair.second->getAllDrivers();
            result.insert(result.end(), childDrivers.begin(), childDrivers.end());
        }
        return result;
    }
};

// Geohash implementation
class Geohash
{
private:
    static const string BASE32;

    static pair<double, double> decodeRange(char c, int bit, double min, double max)
    {
        int index = BASE32.find(c);
        if (index == string::npos)
            return {min, max};

        double mid = (min + max) / 2;
        if ((index & (1 << (4 - bit))) != 0)
        {
            return {mid, max};
        }
        else
        {
            return {min, mid};
        }
    }

public:
    static string encode(double latitude, double longitude, int precision = GEOHASH_PRECISION)
    {
        double latMin = -90.0, latMax = 90.0;
        double lonMin = -180.0, lonMax = 180.0;
        string geohash;
        int bit = 0;
        int ch = 0;

        for (int i = 0; i < precision; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (bit % 2 == 0)
                {
                    // Longitude
                    double mid = (lonMin + lonMax) / 2;
                    if (longitude >= mid)
                    {
                        ch |= (1 << (4 - bit % 5));
                        lonMin = mid;
                    }
                    else
                    {
                        lonMax = mid;
                    }
                }
                else
                {
                    // Latitude
                    double mid = (latMin + latMax) / 2;
                    if (latitude >= mid)
                    {
                        ch |= (1 << (4 - bit % 5));
                        latMin = mid;
                    }
                    else
                    {
                        latMax = mid;
                    }
                }
                bit++;

                if (bit % 5 == 0)
                {
                    geohash += BASE32[ch];
                    ch = 0;
                }
            }
        }

        return geohash;
    }

    static pair<double, double> decode(const string &geohash)
    {
        double latMin = -90.0, latMax = 90.0;
        double lonMin = -180.0, lonMax = 180.0;
        bool isEven = true;

        for (char c : geohash)
        {
            for (int i = 0; i < 5; i++)
            {
                if (isEven)
                {
                    // Longitude
                    auto range = decodeRange(c, i, lonMin, lonMax);
                    lonMin = range.first;
                    lonMax = range.second;
                }
                else
                {
                    // Latitude
                    auto range = decodeRange(c, i, latMin, latMax);
                    latMin = range.first;
                    latMax = range.second;
                }
                isEven = !isEven;
            }
        }

        return {(latMin + latMax) / 2, (lonMin + lonMax) / 2};
    }

    static vector<string> getNeighbors(const string &geohash)
    {
        // For simplicity, we'll just return the geohash with one character less precision
        // In a real implementation, you'd calculate the actual neighboring cells
        if (geohash.length() <= 1)
        {
            return {geohash};
        }

        string prefix = geohash.substr(0, geohash.length() - 1);
        vector<string> neighbors;

        for (char c : BASE32)
        {
            neighbors.push_back(prefix + c);
        }

        return neighbors;
    }
};

const string Geohash::BASE32 = "0123456789bcdefghjkmnpqrstuvwxyz";

// Structure for driver-passenger matching
struct DriverMatch
{
    int driverId;
    double distance;
    chrono::system_clock::time_point lastActive;

    DriverMatch(int id, double dist, chrono::system_clock::time_point time)
        : driverId(id), distance(dist), lastActive(time) {}

    // For min-heap based on distance
    bool operator>(const DriverMatch &other) const
    {
        if (abs(distance - other.distance) < 0.001)
        {
            // If distances are very close, prioritize by wait time
            return lastActive > other.lastActive;
        }
        return distance > other.distance;
    }
};

// Ride-sharing system
class RideSharingSystem
{
private:
    shared_ptr<TrieNode> locationTrie;
    unordered_map<int, shared_ptr<Driver>> drivers;
    unordered_map<int, shared_ptr<Passenger>> pendingRequests;
    unordered_map<int, string> driverGeohashes;
    int nextDriverId;
    int nextPassengerId;

public:
    RideSharingSystem() : locationTrie(make_shared<TrieNode>()), nextDriverId(1), nextPassengerId(1) {}

    int addDriver(double latitude, double longitude)
    {
        int driverId = nextDriverId++;
        auto driver = make_shared<Driver>(driverId, latitude, longitude);
        drivers[driverId] = driver;

        // Add to geohash trie
        string geohash = Geohash::encode(latitude, longitude);
        driverGeohashes[driverId] = geohash;
        locationTrie->insertDriver(geohash, driverId);

        cout << "Added driver #" << driverId << " at location ("
             << latitude << ", " << longitude << ") with geohash " << geohash << endl;

        return driverId;
    }

    void updateDriverLocation(int driverId, double latitude, double longitude)
    {
        if (drivers.find(driverId) == drivers.end())
        {
            cout << "Driver #" << driverId << " not found!" << endl;
            return;
        }

        auto driver = drivers[driverId];

        // Remove from old geohash
        if (driverGeohashes.find(driverId) != driverGeohashes.end())
        {
            locationTrie->removeDriver(driverGeohashes[driverId], driverId);
        }

        // Update location
        driver->updateLocation(latitude, longitude);

        // Add to new geohash
        string geohash = Geohash::encode(latitude, longitude);
        driverGeohashes[driverId] = geohash;
        locationTrie->insertDriver(geohash, driverId);

        cout << "Updated driver #" << driverId << " location to ("
             << latitude << ", " << longitude << ") with geohash " << geohash << endl;
    }

    void setDriverAvailability(int driverId, bool available)
    {
        if (drivers.find(driverId) == drivers.end())
        {
            cout << "Driver #" << driverId << " not found!" << endl;
            return;
        }

        drivers[driverId]->setAvailable(available);
        cout << "Set driver #" << driverId << " availability to "
             << (available ? "available" : "unavailable") << endl;
    }

    int requestRide(double latitude, double longitude)
    {
        
        int passengerId = nextPassengerId++;
        auto passenger = make_shared<Passenger>(passengerId, latitude, longitude);
        pendingRequests[passengerId] = passenger;

        cout << "New ride request #" << passengerId << " at location ("
             << latitude << ", " << longitude << ")" << endl;

        // Try to match with a driver immediately
        matchRideRequest(passengerId);

        return passengerId;
    }

    void matchRideRequest(int passengerId)
    {
        if (pendingRequests.find(passengerId) == pendingRequests.end())
        {
            cout << "Ride request #" << passengerId << " not found!" << endl;
            return;
        }

        auto passenger = pendingRequests[passengerId];
        string passengerGeohash = Geohash::encode(
            passenger->location.latitude,
            passenger->location.longitude);

        cout << "Matching ride request #" << passengerId << " with geohash " << passengerGeohash << endl;
        


        // Find nearby drivers using geohash prefix
        vector<string> nearbyGeohashes = Geohash::getNeighbors(passengerGeohash);
        nearbyGeohashes.push_back(passengerGeohash);

        priority_queue<DriverMatch, vector<DriverMatch>, greater<DriverMatch>> driverHeap;

        for (const auto &geohash : nearbyGeohashes)
        {
            vector<int> nearbyDriverIds = locationTrie->findDriversWithPrefix(geohash.substr(0, 3));

            for (int driverId : nearbyDriverIds)
            {
                if (drivers.find(driverId) != drivers.end() && drivers[driverId]->available)
                {
                    double distance = passenger->location.distanceTo(drivers[driverId]->location);
                    driverHeap.push(DriverMatch(
                        driverId,
                        distance,
                        drivers[driverId]->lastActive));
                }
            }
        }

        if (driverHeap.empty())
        {
            cout << "No available drivers found for ride request #" << passengerId << endl;
            return;
        }

        // Get the best match (nearest driver)
        DriverMatch bestMatch = driverHeap.top();
        int matchedDriverId = bestMatch.driverId;

        // Assign the driver
        drivers[matchedDriverId]->setAvailable(false);
        pendingRequests.erase(passengerId);

        cout << "Matched ride request #" << passengerId << " with driver #"
             << matchedDriverId << " (distance: " << fixed << setprecision(2)
             << bestMatch.distance << " km)" << endl;

             cout <<  "\n\n " << endl;
    }

    void processExpiredRequests()
    {
        vector<int> expiredIds;

        for (const auto &pair : pendingRequests)
        {
            if (pair.second->isExpired())
            {
                expiredIds.push_back(pair.first);
            }
        }

        for (int id : expiredIds)
        {
            cout << "Ride request #" << id << " expired after waiting for "
                 << pendingRequests[id]->getWaitTime() << endl;
            pendingRequests.erase(id);
        }
    }

    void displayStats()
    {
        cout << "\n--- System Statistics ---" << endl;
        cout << "Total Drivers: " << drivers.size() << endl;

        int availableDrivers = 0;
       

        cout << "\t\t\tAvailable Drivers: "  << endl;
        cout << "+----+------------------+-------+----------+----------+" << endl;
        cout << "| ID |       Latitude       |           Longitude     |" << endl;
        cout << "+----+------------------+-------+----------+----------+" << endl;
        for (const auto &pair : drivers)
        {
            if (pair.second->available)
            {
                cout << "| " << (pair.second->id) <<" |       " << pair.second->location.latitude << "       |           " << pair.second->location.longitude <<"     |" << endl;
                // cout <<  << " -> " << (pair.second->location.latitude)  << " :: "<< pair.second->location.longitude << endl;
                availableDrivers++;
            }
        }
        cout << "+----+------------------+-------+----------+----------+" << endl;
        cout << "Total Available Drivers : " << availableDrivers << endl;
        cout << "Pending Ride Requests: " << pendingRequests.size() << endl;

        if (!pendingRequests.empty())
        {
            cout << "\nPending Requests:" << endl;
            for (const auto &pair : pendingRequests)
            {
                cout << "  Request #" << pair.first << " - Waiting for "
                     << pair.second->getWaitTime() << endl;
            }
        }

        cout << "-------------------------------------------------------\n"
             << endl;
             cout << "\n\n " << endl;
    }
};

// Interactive menu
void userMenu(RideSharingSystem &riderSharingSystem)
{

    int choice;

    do
    {
        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "|                             1. Request a ride                                  |" << endl;
        cout << "|                             0. Exit                                            |" << endl;
        cout << "|--------------------------------------------------------------------------------|" << endl;

        cout << "Enter your choice: ";
        cin >> choice;

        switch (choice)
        {

        case 1:
        {
            double lat, lng;
            cout << "Enter passenger latitude: ";
            cin >> lat;
            cout << "Enter passenger longitude: ";
            cin >> lng;
            riderSharingSystem.requestRide(lat, lng);
            break;
        }

        case 0:
            cout << "Exiting..." << endl;
            break;
        default:
            cout << "Invalid choice. Please try again." << endl;
        }
    } while (choice != 0);
}

void adminMenu(RideSharingSystem &riderSharingSystem)
{

    int choice;

    do
    {

        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "|                     === Ride-Sharing System Menu ===                           |" << endl;
        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "|                     1. Add a new driver                                        |" << endl;
        cout << "|                     2. Update driver location                                  |" << endl;
        cout << "|                     3. Set driver availability                                 |" << endl;
        cout << "|                     4. Process expired requests                                |" << endl;
        cout << "|                     5. Display system statistics                               |" << endl;
        cout << "|                     0. Exit                                                    |" << endl;
        cout << "|--------------------------------------------------------------------------------|" << endl;

        cout << "Enter your choice: ";
        cin >> choice;

        switch (choice)
        {
        case 1:
        {
            double lat, lng;
            cout << "Enter latitude: ";
            cin >> lat;
            cout << "Enter longitude: ";
            cin >> lng;
            riderSharingSystem.addDriver(lat, lng);
            break;
        }
        case 2:
        {
            int id;
            double lat, lng;
            cout << "Enter driver ID: ";
            cin >> id;
            cout << "Enter new latitude: ";
            cin >> lat;
            cout << "Enter new longitude: ";
            cin >> lng;
            riderSharingSystem.updateDriverLocation(id, lat, lng);
            break;
        }
        case 3:
        {
            int id;
            char status;
            cout << "Enter driver ID: ";
            cin >> id;
            cout << "Available (y/n): ";
            cin >> status;
            riderSharingSystem.setDriverAvailability(id, status == 'y' || status == 'Y');
            break;
        }

        case 4:
            riderSharingSystem.processExpiredRequests();
            break;
        case 5:
            riderSharingSystem.displayStats();
            break;

        case 0:
            cout << "Exiting..." << endl;
            break;
        default:
            cout << "Invalid choice. Please try again." << endl;
        }
    } while (choice != 0);
}
int main()
{
    system("cls");
    int choice;
    RideSharingSystem riderSharingSystem;
    do
    {
        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "|                     === Ride-Sharing System Menu ===                           |" << endl;
        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "|                             1. Admin                                           |" << endl;
        cout << "|                             2. User                                            |" << endl;
        cout << "|                             0. Exit                                            |" << endl;
        cout << "|--------------------------------------------------------------------------------|" << endl;
        cout << "Enter choice : ";
        cin >> choice;
        system("cls");
        switch (choice)
        {
        case 1:
        {
            string username = "";
            string password = "";
            cout << "ENter username : ";
            cin >> username;
            cout << "Enter password : ";
            cin >> password;

            if (username == "admin" and password == "admin")
            {
                adminMenu(riderSharingSystem);
                system("cls");
            }
            else
            {
                cout << "Authentication Failed" << endl;
                system("cls");
                break;
            }

            // int adminChoice;

            // do
            // {
        
            //     cout << "|--------------------------------------------------------------------------------|" << endl;
            //     cout << "|                     === Ride-Sharing System Menu ===                           |" << endl;
            //     cout << "|--------------------------------------------------------------------------------|" << endl;
            //     cout << "|                     1. Add a new driver                                        |" << endl;
            //     cout << "|                     2. Update driver location                                  |" << endl;
            //     cout << "|                     3. Set driver availability                                 |" << endl;
            //     cout << "|                     4. Process expired requests                                |" << endl;
            //     cout << "|                     5. Display system statistics                               |" << endl;
            //     cout << "|                     0. Exit                                                    |" << endl;
            //     cout << "|--------------------------------------------------------------------------------|" << endl;
        
            //     cout << "Enter your Choice: ";
            //     cin >> adminChoice;
        
            //     switch (adminChoice)
            //     {
            //     case 1:
            //     {
            //         double lat, lng;
            //         cout << "Enter latitude: ";
            //         cin >> lat;
            //         cout << "Enter longitude: ";
            //         cin >> lng;
            //         riderSharingSystem.addDriver(lat, lng);
            //         break;
            //     }
            //     case 2:
            //     {
            //         int id;
            //         double lat, lng;
            //         cout << "Enter driver ID: ";
            //         cin >> id;
            //         cout << "Enter new latitude: ";
            //         cin >> lat;
            //         cout << "Enter new longitude: ";
            //         cin >> lng;
            //         riderSharingSystem.updateDriverLocation(id, lat, lng);
            //         break;
            //     }
            //     case 3:
            //     {
            //         int id;
            //         char status;
            //         cout << "Enter driver ID: ";
            //         cin >> id;
            //         cout << "Available (y/n): ";
            //         cin >> status;
            //         riderSharingSystem.setDriverAvailability(id, status == 'y' || status == 'Y');
            //         break;
            //     }
        
            //     case 4:
            //         riderSharingSystem.processExpiredRequests();
            //         break;
            //     case 5:
            //         riderSharingSystem.displayStats();
            //         break;
        
            //     case 0:
            //         cout << "Exiting..." << endl;
            //         break;
            //     default:
            //         cout << "Invalid Choice. Please try again." << endl;
            //     }
            // } while (adminChoice != 0);
            break;
        }   
            /* code */

        case 2:
        {

            // int userChoice;

            // do
            // {
            //     cout << "|--------------------------------------------------------------------------------|" << endl;
            //     cout << "|                             1. Request a ride                                  |" << endl;
            //     cout << "|                             0. Exit                                            |" << endl;
            //     cout << "|--------------------------------------------------------------------------------|" << endl;

            //     cout << "Enter your choice: ";
            //     cin >> userChoice;

            //     switch (userChoice)
            //     {

            //     case 1:
            //     {
            //         double lat, lng;
            //         cout << "Enter passenger latitude: ";
            //         cin >> lat;
            //         cout << "Enter passenger longitude: ";
            //         cin >> lng;
            //         riderSharingSystem.requestRide(lat, lng);
            //         break;
            //     }

            //     case 0:
            //         cout << "Exiting..." << endl;
            //         break;
            //     default:
            //         cout << "Invalid userChoice. Please try again." << endl;
            //     }
            // } while (userChoice != 0);
            userMenu(riderSharingSystem);
            system("cls");
        }
        case 0:
            break;
        default:
            break;
        }

    } while (choice != 0);
    return 0;
}
