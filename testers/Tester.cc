#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <sstream> 
#include <string>
#include <vector>
#include <thread>
using namespace std;

string J1;
string J2;
string J3;
string J4;

int max(int s1, int s2, int s3, int s4) {
    const vector<int> v = {s1,s2,s3,s4};
    int m = 0;
    if(s2>v[m]) m = 1;
    if(s3>v[m]) m = 2;
    if(s4>v[m]) m = 3;
    return m;
}

void threadf(int seed, int* result){
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
    
    *result = max(s1,s2,s3,s4);
}

int main(){
    cout << "Lista de jugadores disponibles: " << endl;
    system("./Game -l");
    
    int n,seed;
    cout << "Nombre de los jugadores:" << endl;
    cin >> J1 >> J2 >> J3 >> J4;
    cout << "Número de rondas:" << endl;
    cin >> n;
    cout << "Seed inicial (las demas seeds serán seed+1, seed+2, ..., seed+n-1):" << endl;
    cin >> seed;
    cout << "Procesando..." << endl;
    vector<thread> threads = vector<thread>(0);
    int* resultats;
    resultats = (int*)malloc(sizeof(int) * n);
    for(int i=0; i<n; ++i) {
        thread t(threadf, i+seed, &resultats[i]);
        threads.push_back(move(t));
    }
    

    for(int i=0; i<n; ++i) {
        threads[i].join();
    }
    
    int w1,w2,w3,w4;
    w1=w2=w3=w4=0;
    for(int i=0; i<n; ++i){
        if(resultats[i] == 0) ++w1;
        else if(resultats[i] == 1) ++w2;
        else if(resultats[i] == 2) ++w3;
        else if(resultats[i] == 3) ++w4;
    }
    
    for(int i=0; i<n; ++i){
        string c = "rm " + to_string(i+seed) + ".res";
        char char_array[c.length()+1];
        strcpy(char_array, c.c_str());
        system(char_array);
    }
    
    cout << "Los resultados son:" << endl;
    cout << J1 << ": " << w1 << endl;
    cout << J2 << ": " << w2 << endl;
    cout << J3 << ": " << w3 << endl;
    cout << J4 << ": " << w4 << endl;
}
