#include <iostream>
#include <sstream>
#include <vector>

using namespace std;


vector<string> strcut(const string& str, char delimiter) {
    vector<string> result;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}

template<typename container>
void parr(const container& c) {
    for (const auto& i : c) {
        cout << i << " ";
    }
}

int main() {


    string line = "hello world from me a h m a d 2010 6 6 ";
    parr(strcut(line, ' '));


    return 0;
}