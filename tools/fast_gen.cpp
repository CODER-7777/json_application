#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <iomanip>
#include <sstream>

using namespace std;

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <target_mb> <output_file>\n";
        return 1;
    }
    
    long long target_bytes = stoll(argv[1]) * 1024 * 1024;
    string filename = argv[2];
    
    ofstream out(filename);
    out << "[\n";
    
    vector<string> categories = {"Electronics", "Books", "Clothes", "Food", "Travel", "Home", "Sports"};
    
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> amount_dist(10.0, 999.99);
    uniform_int_distribution<> cat_dist(0, categories.size() - 1);
    uniform_int_distribution<> status_dist(0, 99); 
    
    long long current_bytes = 2; // "[\n"
    bool first = true;
    long long tx_id = 1;
    
    while (true) {
        double amount = amount_dist(gen);
        string category = categories[cat_dist(gen)];
        string status = (status_dist(gen) < 18) ? "failed" : "completed";
        
        stringstream ss;
        ss << "  {\n"
           << "    \"id\": \"tx_" << tx_id++ << "\",\n"
           << "    \"timestamp\": \"2026-06-01T12:00:00Z\",\n"
           << "    \"amount\": " << fixed << setprecision(2) << amount << ",\n"
           << "    \"category\": \"" << category << "\",\n"
           << "    \"status\": \"" << status << "\"\n"
           << "  }";
           
        string obj = ss.str();
        
        // Include the potential comma space
        long long space_needed = obj.size() + (first ? 0 : 2);
        
        if (current_bytes + space_needed >= target_bytes) {
            break; 
        }
        
        if (!first) {
            out << ",\n";
            current_bytes += 2;
        }
        first = false;
        
        out << obj;
        current_bytes += obj.size();
    }
    
    out << "\n]\n";
    out.close();
    
    return 0;
}
