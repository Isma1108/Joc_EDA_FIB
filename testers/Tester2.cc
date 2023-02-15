#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <sstream> 
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
using namespace std;

string J1;
string J2;
string J3;
string J4;

int pos(int s1, int s2, int s3, int s4, vector<int>& v, int i) {
    if (i == 0)
    {
        if (s1 == v[0]) return 0;
        if (s1 == v[1]) return 1;
        if (s1 == v[2]) return 2;
        if (s1 == v[3]) return 3;
    }
    else if (i == 1)
    {
        if (s2 == v[0]) return 0;
        if (s2 == v[1]) return 1;
        if (s2 == v[2]) return 2;
        if (s2 == v[3]) return 3;
    }
    else if (i == 2)
    {
        if (s3 == v[0]) return 0;
        if (s3 == v[1]) return 1;
        if (s3 == v[2]) return 2;
        if (s3 == v[3]) return 3;
    }
    else if (i == 3)
    {
        if (s4 == v[0]) return 0;
        if (s4 == v[1]) return 1;
        if (s4 == v[2]) return 2;
        if (s4 == v[3]) return 3;
    }
    return 0;
}

void threadf(int seed, int results[4]){
    //Executar commanda
    string c = "./Game " + J1 + " " + J2 + " " + J3 + " " + J4 + " -s " + to_string(seed) + " < default.cnf > " + to_string(seed) + ".res 2> /dev/null";
    char char_array[c.length()+1];
    strcpy(char_array, c.c_str());
    system(char_array);
    
    //Llegir resultat
    ifstream f = ifstream(to_string(seed)+".res");
    string temp;

    while(getline(f, temp)){
        if (temp.find("round 200") != string::npos) break;
    }
    getline(f, temp); //Llegim espai en blanc
    getline(f, temp); //Llegim score
    
    stringstream X(temp);
    string part;
    char tab = 9;
    getline(X, part, tab);
    getline(X, part, tab);
    
    int s1,s2,s3,s4;
    s1 = stoi(part);
    getline(X, part, tab);
    s2 = stoi(part);
    getline(X, part, tab);
    s3 = stoi(part);
    getline(X, part, tab);
    s4 = stoi(part);

    vector<int> v = {s1,s2,s3,s4};
    sort(v.begin(), v.end(), greater<int>());
    results[0] = pos(s1,s2,s3,s4, v, 0);
    results[1] = pos(s1,s2,s3,s4, v, 1);
    results[2] = pos(s1,s2,s3,s4, v, 2);
    results[3] = pos(s1,s2,s3,s4, v, 3);
}

int main(){
    cout << "Llista de jugadors disponibles: " << endl;
    system("./Game -l");
    
    int n,seed;
    cout << "Nom dels jugadors:" << endl;
    cin >> J1 >> J2 >> J3 >> J4;
    cout << "Numero de rondes:" << endl;
    cin >> n;
    cout << "Seed inicial (després seed, seed+1, seed+2, ..., seed+n-1):" << endl;
    cin >> seed;
    cout << "Calculant..." << endl;
    vector<thread> threads = vector<thread>(0);
    int resultats [n][4];
    for(int i=0; i<n; ++i) {
        thread t(threadf, i+seed, resultats[i]);
        threads.push_back(move(t));
    }
    

    for(int i=0; i<n; ++i) {
        threads[i].join();
    }
    
    int w1,w2,w3,w4;
    int punts;
    for (int j = 0; j < 4; ++j)
    {
        w1=w2=w3=w4=0;
        punts = 0;
        for(int i=0; i<n; ++i){
            if(resultats[i][j] == 0) {++w1; punts += 4;}
            else if(resultats[i][j] == 1) {++w2; punts += 3;}
            else if(resultats[i][j] == 2) {++w3; punts += 2;}
            else if(resultats[i][j] == 3) {++w4; punts++;}
        }

        cout << "Jugador "<< j+1 <<" ha quedat" << endl;
        cout << "Primer" << ": " << w1 << endl;
        cout <<  "Segon" << ": " << w2 << endl;
        cout << "Tercer" << ": " << w3 << endl;
        cout << "Quart" << ": " << w4 << endl;
        cout << "Punts totals: " << punts << endl; 
        cout << endl;
    }
    
    
    for(int i=0; i<n; ++i){
        string c = "rm " + to_string(i+seed) + ".res";
        char char_array[c.length()+1];
        strcpy(char_array, c.c_str());
        system(char_array);
    }
    
}
