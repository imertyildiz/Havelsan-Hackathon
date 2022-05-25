#include <bits/stdc++.h>

using namespace std;

enum TrainType 
{
    Load,
    Main,
    Fast
};

enum Direction
{
    Right,
    Left
};

// ----------------- Globals
int numTLoad, numTMain, numTFast, numDays, totalTime;
float mutate_chance = 0.05f;
int sample_size = 10;
int parent_num = 2;
int max_generation = 5;
int fill_rate;
vector<string> trains;
vector<long long> prevFitness;

int conj_pen = -40000; // use map
int resend_pen = -40000; // check before
int wrnsend_pen = -30000; // chcek before dir
int rep_pen = -7000;  // count old runs
int missing_pen = -7000; // check every 1440

typedef struct TLoad
{
    float speed = 75.0f;
    int repair_km = 3000;
    int repair_time = 2160;
    int rest_time = 180;
    int profit = 50000;
} TLoad;

typedef struct TMain
{
    float speed = 100.0f;
    int repair_km = 2500;
    int repair_time = 1440;
    int rest_time = 240;
    int profit = 45000;
} TMain;

typedef struct TFast
{
    float speed = 200.0f;
    int repair_km = 6000;
    int repair_time = 720;
    int rest_time = 120;
    int profit = 60000;
} TFast;

typedef struct Station
{
    char name;
    set<pair<int,int>> closed;
} Station;

typedef struct Gene
{
    string train_id;
    Direction direction;
    Gene()
    {
        if (rand()%1440 < fill_rate)
            train_id = trains[rand()%trains.size()];
        else
            train_id = "";
        direction = static_cast<Direction>(rand()%2);
    }
    Gene(int i)
    {
        if (i == 0)
            train_id = "";
        else
            train_id = trains[rand()%trains.size()];
        direction = static_cast<Direction>(rand()%2);
    }
    int Prize()
    {
        string prefix = train_id.substr(0,2);
        if (prefix == "HT")
            return 60000;
        else if (prefix == "AT")
            return 45000;
        else
            return 50000;
    }
    // bool operator==(const Gene& rhs)
    // {
    //     return train_id == rhs.train_id && direction == rhs.direction;
    // }
} Gene;

typedef struct Chromosome
{
    vector<Gene> genes;
    long long fitness;
    Chromosome()
    {
        for (int i = 0; i < totalTime; i++)
            genes.push_back(Gene());
        fitness = 0;
    }
    Chromosome(int i)
    {
        for (int i = 0; i < totalTime; i++)
        {
            genes.push_back(Gene(0));
            if (i%610 == 0)
            {
                genes.push_back(Gene(1));
            }
        }
    }
    // bool operator==(const Chromosome& rhs)
    // {   
    //     return genes == rhs.genes && fitness == rhs.fitness;
    // }
} Chromosome;

int fill_conj(string type, Direction dir, int start_time);

unordered_map<string, set<pair<int, int>>> stations = 
{
    {"A", {}}, {"B", {}}, {"C", {}}, {"D", {}}, {"E", {}}, {"F", {}}, {"O", {}},
    {"G", {}}, {"H", {}}, {"I", {}}, {"J", {}}, {"K", {}}, {"L", {}},
    {"N", {}}, {"P", {}}, {"R", {}}, {"S", {}}
};

void clearStations()
{
    for (auto it = stations.begin(); it != stations.end(); it++)
    {
        it->second.clear();
    }
}

bool is_conj(pair<int,int> interval, string station)
{
    for (auto it = stations[station].begin(); it != stations[station].end(); it++)
    {
        if (it->second < interval.first)
            continue;
        else if (it->first > interval.second)
            break;
        else
            return true;
    }
    return false;
}

vector<Chromosome> createChromosomes()
{
    vector<Chromosome> chromosomes;
    int mid = sample_size/2;
    for (int i = 0; i < mid; i++)
    {
        chromosomes.push_back(Chromosome());
    }
    for (int i = mid; i < sample_size; i++)
    {
        chromosomes.push_back(Chromosome(1));
    }
    return chromosomes;
}

int conjuction(vector<Gene> genes, int idx)
{
    int conj = fill_conj(genes[idx].train_id.substr(0,2), genes[idx].direction, idx);
    return conj * conj_pen;
}

int resend(vector<Gene> genes, int idx)
{
    string type = genes[idx].train_id.substr(0,2);
    if (type == "HT")
    {
        for (int i = 1; i <= 330; i++)
        {
            if (idx - i < 0)
                break;
            if (genes[idx-i].train_id == genes[idx].train_id &&
            genes[idx-i].direction == genes[idx].direction)
            {
                return resend_pen;
            }
        }
    }
    else if (type == "AT")
    {
        for (int i = 1; i < 610; i++)
        {
            if (idx - i < 0)
                break;
            if (genes[idx-i].train_id == genes[idx].train_id &&
            genes[idx-i].direction == genes[idx].direction)
            {
                return resend_pen;
            }
        }
    }
    else
    {
        for (int i = 1; i < 595; i++)
        {
            if (idx - i < 0)
                break;
            if (genes[idx-i].train_id == genes[idx].train_id &&
            genes[idx-i].direction == genes[idx].direction)
            {
                return resend_pen;
            }
        }
    }
    return 0;
}

int wrongsnd(vector<Gene> genes, int idx)
{
    bool left = genes[idx].direction == Direction::Left;
    int inversePre = 0; // -1: inverse, 1: same, 0: no preroute
    for (int i = idx-1; i >= 0; i--)
    {
        if (genes[i].train_id == genes[idx].train_id)
        {
            if (genes[i].direction != genes[idx].direction)
                inversePre = -1;
            else
                inversePre = 1;
        }
    }

    if (left && inversePre == 0)
        return wrnsend_pen;
    else if (inversePre == 1)
        return wrnsend_pen;
    return 0;
}

int unrepaired(vector<Gene> genes, int idx)
{
    int count = 1;
    int max_rest = 0;
    int rest = 0;
    int rest_time;
    int rest_count;
    string type = genes[idx].train_id.substr(0,2);
    if (type == "HT")
    {
        rest_time = 720;
        rest_count = 14;
    }
    else if (type == "AT")
    {
        rest_time = 1440;
        rest_count = 6;
    }
    else
    {
        rest_time = 2160;
        rest_count = 8;
    }

    for (int i = idx-1; i >= 0; i--)
    {
        if (genes[i].train_id == genes[idx].train_id)
        {
            count++;
        }
    }
    for (int i = idx-1; i >= 0; i--)
    {
        if (genes[i].train_id == genes[idx].train_id)
        {
            if (rest > max_rest)
                max_rest = rest;
            rest = 0;
            count++;
        }
        else
        {
            rest++;
        }
    }

    if (count > rest_count && max_rest < rest_time)
        return rep_pen;
    return 0;
}

int missing(vector<Gene> genes, int idx)
{
    int Loadlast = 1, Mainlast = 1, Fastlast = 2;
    for (int i = 0; i < 1440; i++)
    {
        if (genes[idx-i].train_id != "")
        {
            string type = genes[idx-i].train_id.substr(0,2);
            if (type == "HT")
                Fastlast--;
            if (type == "AT")
                Mainlast--;
            if (type == "YT")
                Loadlast--;
        }
    }
    if (Loadlast > 0 || Mainlast > 0 || Fastlast > 0)
        return missing_pen;
    return 0;
}

int insertInterval(string station, pair<int,int> interval)
{
    int conj = 0;
    if (is_conj(interval,station)) conj = 1;
    stations[station].insert(interval);
    return conj;
}

int fill_conj(string type, Direction dir, int start_time)
{
    int conj = 0;
    if (type == "HT")
    {
        if (dir == Direction::Right)
        {
            //stations["B"].insert(make_pair(start_time+30,start_time+45));
            conj += insertInterval("B", make_pair(start_time+30,start_time+45));
            //stations["C"].insert(make_pair(start_time+67,start_time+83));
            conj += insertInterval("C", make_pair(start_time+67,start_time+83));
            //stations["D"].insert(make_pair(start_time+112,start_time+133));
            conj += insertInterval("D", make_pair(start_time+112,start_time+133));
            //stations["E"].insert(make_pair(start_time+150,start_time+165));
            conj += insertInterval("E", make_pair(start_time+150,start_time+165));
            //stations["F"].insert(make_pair(start_time+187,start_time+208));
            conj += insertInterval("F", make_pair(start_time+187,start_time+208));
            //stations["O"].insert(make_pair(start_time+210,start_time+225));
            conj += insertInterval("O", make_pair(start_time+210,start_time+225));
        }
        else
        {
            // stations["F"].insert(make_pair(start_time+7,start_time+28));
            conj += insertInterval("F", make_pair(start_time+7,start_time+28));
            // stations["E"].insert(make_pair(start_time+45,start_time+60));
            conj += insertInterval("E", make_pair(start_time+45,start_time+60));
            // stations["D"].insert(make_pair(start_time+82,start_time+103));
            conj += insertInterval("D", make_pair(start_time+82,start_time+103));
            // stations["C"].insert(make_pair(start_time+127,start_time+143));
            conj += insertInterval("C", make_pair(start_time+127,start_time+143));
            // stations["B"].insert(make_pair(start_time+165,start_time+180));
            conj += insertInterval("B", make_pair(start_time+165,start_time+180));
            // stations["A"].insert(make_pair(start_time+210,start_time+225));
            conj += insertInterval("A", make_pair(start_time+210,start_time+225));
        }
    }
    else if (type == "AT")
    {
        if (dir == Direction::Right)
        {
            // stations["K"].insert(make_pair(start_time+60,start_time+85));
            conj += insertInterval("K", make_pair(start_time+60,start_time+85));
            // stations["P"].insert(make_pair(start_time+140,start_time+160));
            conj += insertInterval("P", make_pair(start_time+140,start_time+160));
            // stations["R"].insert(make_pair(start_time+205,start_time+225));
            conj += insertInterval("R", make_pair(start_time+205,start_time+225));
            // stations["D"].insert(make_pair(start_time+255,start_time+280));
            conj += insertInterval("D", make_pair(start_time+255,start_time+280));
            // stations["S"].insert(make_pair(start_time+305,start_time+325));
            conj += insertInterval("S", make_pair(start_time+305,start_time+325));
            // stations["P"].insert(make_pair(start_time+370,start_time+395));
            conj += insertInterval("P", make_pair(start_time+370,start_time+395));
        }
        else
        {
            // stations["S"].insert(make_pair(start_time+45,start_time+65));
            conj += insertInterval("S", make_pair(start_time+45,start_time+65));
            // stations["D"].insert(make_pair(start_time+95,start_time+120));
            conj += insertInterval("D", make_pair(start_time+95,start_time+120));
            // stations["R"].insert(make_pair(start_time+145,start_time+165));
            conj += insertInterval("R", make_pair(start_time+145,start_time+165));
            // stations["P"].insert(make_pair(start_time+210,start_time+235));
            conj += insertInterval("P", make_pair(start_time+210,start_time+235));
            // stations["K"].insert(make_pair(start_time+290,start_time+310));
            conj += insertInterval("K", make_pair(start_time+290,start_time+310));
            // stations["N"].insert(make_pair(start_time+370,start_time+390));
            conj += insertInterval("N", make_pair(start_time+370,start_time+390));
        }
    }
    else 
    {
        if (dir == Direction::Right)
        {
            // stations["H"].insert(make_pair(start_time+61,start_time+62));
            conj += insertInterval("H", make_pair(start_time+61,start_time+62));
            // stations["I"].insert(make_pair(start_time+127,start_time+128));
            conj += insertInterval("I", make_pair(start_time+127,start_time+128));
            // stations["F"].insert(make_pair(start_time+167,start_time+168));
            conj += insertInterval("F", make_pair(start_time+167,start_time+168));
            // stations["J"].insert(make_pair(start_time+244,start_time+245));
            conj += insertInterval("J", make_pair(start_time+244,start_time+245));
            // stations["K"].insert(make_pair(start_time+324,start_time+325));
            conj += insertInterval("K", make_pair(start_time+324,start_time+325));
            // stations["L"].insert(make_pair(start_time+414,start_time+415));
            conj += insertInterval("L", make_pair(start_time+414,start_time+415));
        }
        else
        {
            // stations["K"].insert(make_pair(start_time+89,start_time+90));
            conj += insertInterval("K", make_pair(start_time+89,start_time+90));
            // stations["J"].insert(make_pair(start_time+169,start_time+170));
            conj += insertInterval("J", make_pair(start_time+169,start_time+170));
            // stations["F"].insert(make_pair(start_time+247,start_time+248));
            conj += insertInterval("F", make_pair(start_time+247,start_time+248));
            // stations["I"].insert(make_pair(start_time+287,start_time+288));
            conj += insertInterval("I", make_pair(start_time+287,start_time+288));
            // stations["H"].insert(make_pair(start_time+352,start_time+353));
            conj += insertInterval("H", make_pair(start_time+352,start_time+353));
            // stations["G"].insert(make_pair(start_time+414,start_time+415));
            conj += insertInterval("G", make_pair(start_time+414,start_time+415));
        }
    }
    return conj;
}

long long evaluate(vector<Gene> genes)
{
    long long fitness = 0;
    for (int i = 0; i < genes.size(); i++)
    {
        if (genes[i].train_id != "")
        {
            fitness += genes[i].Prize();
            fitness += conjuction(genes, i);
            fitness += resend(genes, i);
            fitness += wrongsnd(genes, i);
            fitness += unrepaired(genes, i);
            if (i % 1440 == 1439)
            {
                missing(genes, i);
            }
        }
        fill_conj(genes[i].train_id.substr(0,2), genes[i].direction, i);
    }
    return fitness;
}

void evaluateAll(vector<Chromosome>& chromosomes)
{
    prevFitness.clear();
    for (int i = 0; i < chromosomes.size(); i++)
    {
        prevFitness.push_back(chromosomes[i].fitness);
        chromosomes[i].fitness = evaluate(chromosomes[i].genes);
        clearStations();
    }
}

bool chr_compare(Chromosome c1, Chromosome c2)
{
    return c1.fitness < c2.fitness;
}

vector<Chromosome> selection(vector<Chromosome> chromosomes)
{
    //cout << "Entered selection" << endl;
    // returns indices by using roulette wheel method for
    // breeding and indices for killing at last
    vector<Chromosome> res;
    long long min_f = INT_MAX;
    for (Chromosome chr : chromosomes)
    {
        if (chr.fitness < min_f)
            min_f = chr.fitness;
    }
    long long total_f = 0;
    for (Chromosome chr : chromosomes)
        total_f += chr.fitness - min_f;
    sort(chromosomes.begin(), chromosomes.end(), chr_compare);
    for (int i = 0; i < chromosomes.size(); i++)
    {
        //cout << "Chance: " << ((double)chromosomes[i].fitness-min_f)/total_f;
        if ((double)rand()/RAND_MAX < ((double)chromosomes[i].fitness-min_f)*sample_size/total_f)
        {
            //cout << it->first << " " << it->second->genes.size() << endl;
            res.push_back(chromosomes[i]);
            if (res.size() == parent_num)
                break;
        }
    }
    for (int i = chromosomes.size()-1; i >= 0; i--)
    {
        if (((double)rand()/RAND_MAX) < (((double)chromosomes[i].fitness-min_f)*sample_size/total_f))
        {
            res.push_back(chromosomes[i]);
            if (res.size() == parent_num*3/2)
                break;
        }
    }

    //cout << "Exited selection : " << res.size() << endl;
    return res;
}

void crossover(vector<Chromosome>& chromosomes, Chromosome c1, Chromosome c2)
{
    //cout << "Entered crossover" << endl;
    Chromosome child = Chromosome();
    int midpoint = rand()%totalTime;
    //cout << midpoint << endl;
    for (int i = 0; i < midpoint; i++)
    {
        child.genes[i] = c1.genes[i];
    }
    //cout << "First for" << endl;
    for (int i = midpoint; i < totalTime; i++)
    {
        child.genes[i] = c2.genes[i];
    }
    //cout << "Exited crossover" << endl;
    chromosomes.push_back(child);
}

void kill(vector<Chromosome>& chromosomes, Chromosome c)
{
    long long ftn = c.fitness;
    for (int i = 0; i < chromosomes.size(); i++)
    {
        if (chromosomes[i].fitness == ftn)
        {
            chromosomes.erase(chromosomes.begin() + i);
            break;
        }
    }
}

void mutation(vector<Chromosome>& chromosomes, float mutate_chance)
{
    for (int i = 0; i < chromosomes.size(); i++)
    {
        for (int j = 0; j < chromosomes[i].genes.size(); j++)
        {
            if (rand()%100 == 0)
            {
                chromosomes[i].genes[j] = Gene();
            }
        }
    }
}

void initTrains()
{
    for (int i = 1; i <= numTLoad; i++)
    {
        trains.push_back("YT-" + to_string(i));
    }
    for (int i = 1; i <= numTMain; i++)
    {
        trains.push_back("AT-" + to_string(i));
    }
    for (int i = 1; i <= numTFast; i++)
    {
        trains.push_back("HT-" + to_string(i));
    }
}

bool converge(vector<Chromosome> chromosomes)
{
    //cout << "Entered converge" << endl;
    long long treshold = 5000;
    long long max_cur = INT_MIN;
    long long max_prev = INT_MIN;
    for (int i = 0; i < chromosomes.size(); i++)
    {
        if (chromosomes[i].fitness > max_cur)
            max_cur = chromosomes[i].fitness;
    }
    for (int i = 0; i < prevFitness.size(); i++)
    {
        if (prevFitness[i] > max_prev)
            max_prev = prevFitness[i];
    }
    if (abs(max_cur - max_prev) < treshold)
    {
        //cout << "Exited converge" << endl;
        return true;
    }
    //cout << "Exited converge" << endl;
    return false;
}

void planned_trains(Chromosome chromosome){
    for(string train: trains){
        int run_count=0;
        if("HT" == train.substr(0,2)){
            cout << "Sefer Adı\t" << train << " A-O Güzergahı" << endl; 
        }
        else if ("YT" == train.substr(0,2)){
            cout << "Sefer Adı\t" << train << " G-L Güzergahı" << endl; 
        }
        else{
            cout << "Sefer Adı\t" << train << " M-P Güzergahı" << endl; 
        }
        for(int i =0; i < chromosome.genes.size(); i++){
            if(chromosome.genes[i].train_id == train){
                run_count++;
                if("HT" == (chromosome.genes[i].train_id.substr(0,2))){
                    int hour=((i%1440)/60);
                    int minute=((i%1440)%60);
                    int hour_1=0, minute_1=0;
                    cout<< "Gün "<< (i/1440)+1 << " - " << "Sefer-" << run_count << ". Bakımdan Sonra katedilen KM: " << (run_count%14)*450<<endl; 
                    cout << "İstasyon\t" << "Kaçıncı KM\t" << "Ortalama Hız\t" << "Varış\t" << "Çıkış\t" << "Bekleme süresi" << endl;
                    cout << "A\t" << 0   <<"\t"<< "-\t" << "-\t" << hour <<":" << minute <<"\t-" <<endl;
                    hour = (hour + ((minute+30)/60))%24;
                    minute = (minute+30)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;
                    cout << "B\t" << 100 <<"\t"<< 200 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour_1 + ((minute_1+22)/60))%24;
                    minute = (minute_1+22)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;
                    cout << "C\t" << 175 <<"\t"<< 120 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour_1 + ((minute_1+30)/60))%24;
                    minute = (minute_1+30)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;
                    cout << "D\t" << 275 <<"\t"<< 147 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour_1 + ((minute_1+22)/60))%24;
                    minute = (minute_1+22)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;                   
                    cout << "E\t" << 350 <<"\t"<< 140 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour_1 + ((minute_1+22)/60))%24;
                    minute = (minute_1+22)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;
                    cout << "F\t" << 425 <<"\t"<< 136 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour_1 + ((minute_1+7)/60))%24;
                    minute = (minute_1+7)%60;
                    hour_1 = (hour + ((minute+15)/60))%24;
                    minute_1 = (minute+15)%60;
                    cout << "O\t" << 450 <<"\t"<< 129 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                }
                else if ("YT" == chromosome.genes[i].train_id.substr(0,2)){
                    int hour=((i%1440)/60);
                    int minute=((i%1440)%60);
                    int hour_1=0, minute_1=0;
                    cout<< "Gün "<< (i/1440)+1 << " - " << "Sefer-" << run_count << ". Bakımdan Sonra katedilen KM: " << (run_count%14)*450<<endl; 
                    cout << "İstasyon\t" << "Kaçıncı KM\t" << "Ortalama Hız\t" << "Varış\t" << "Çıkış\t" << "Bekleme süresi" << endl;
                    cout << "G\t" << 0   <<"\t"<< "-\t" << "-\t" << hour <<":" << minute <<"\t-" <<endl;
                    hour = (hour + ((minute+61)/60))%24;
                    minute = (minute+61)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;
                    cout << "H\t" << 77 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+65)/60))%24;
                    minute = (minute+65)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;
                    cout << "I\t" << 159 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+40)/60))%24;
                    minute = (minute+40)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;
                    cout << "F\t" << 209 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+77)/60))%24;
                    minute = (minute+77)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;                   
                    cout << "J\t" << 306 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+80)/60))%24;
                    minute = (minute+80)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;
                    cout << "K\t" << 406 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+89)/60))%24;
                    minute = (minute+89)%60;
                    hour_1 = (hour + ((minute+0)/60))%24;
                    minute_1 = (minute+0)%60;
                    cout << "L\t" << 518 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                }
                else{
                    int hour=((i%1440)/60);
                    int minute=((i%1440)%60);
                    int hour_1=0, minute_1=0;
                    cout<< "Gün "<< (i/1440)+1 << " - " << "Sefer-" << run_count << ". Bakımdan Sonra katedilen KM: " << (run_count%14)*450<<endl; 
                    cout << "İstasyon\t" << "Kaçıncı KM\t" << "Ortalama Hız\t" << "Varış\t" << "Çıkış\t" << "Bekleme süresi" << endl;
                    cout << "N\t" << 0   <<"\t"<< "-\t" << "-\t" << hour <<":" << minute <<"\t-" <<endl;
                    hour = (hour + ((minute+60)/60))%24;
                    minute = (minute+60)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;
                    cout << "K\t" << 100 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+60)/60))%24;
                    minute = (minute+60)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;
                    cout << "P\t" << 85 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+45)/60))%24;
                    minute = (minute+45)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;
                    cout << "R\t" << 80 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+30)/60))%24;
                    minute = (minute+30)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;                   
                    cout << "D\t" << 76 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+30)/60))%24;
                    minute = (minute+30)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;
                    cout << "S\t" << 74 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl;
                    hour = (hour + ((minute+45)/60))%24;
                    minute = (minute+45)%60;
                    hour_1 = (hour + ((minute+20)/60))%24;
                    minute_1 = (minute+20)%60;
                    cout << "L\t" << 73 <<"\t"<< 75 <<"\t"<<hour<<":"<<minute<<"\t"<< hour_1<<":"<<minute_1 <<"\t15 Dak"<<endl; 
                }
            }
        } 
    }
}

int main()
{
    cout << "Yük treni adedi giriniz: " << endl;
    cin >> numTLoad;
    cout << "Anahat treni adedi giriniz: " << endl;
    cin >> numTMain;
    cout << "Hızlı tren adedi giriniz: " << endl;
    cin >> numTFast;
    cout << "Kaç günlük sefer planlaması istiyorsunuz? " << endl;
    cin >> numDays;

    totalTime = numDays * 24 * 60;
    fill_rate = (numTFast*3 + numTMain*1 + numTLoad*1);

    initTrains();

    vector<Chromosome> chromosomes = createChromosomes();
    evaluateAll(chromosomes);
    // cout << "Fitnesses: " << endl;
    // for (Chromosome c : chromosomes)
    //     cout << c.fitness << endl;
    int generation = 0;
    while(!converge(chromosomes))
    {
        cout << generation << endl;
        vector<Chromosome> selected = selection(chromosomes);
        for (int i = 0; i < parent_num; i+=2)
        {
            crossover(chromosomes, selected[i], selected[i+1]);
        }
        for (int i = parent_num; i < selected.size(); i++)
        {
            kill(chromosomes, selected[i]);
        }
        mutation(chromosomes, mutate_chance);
        evaluateAll(chromosomes);
        //cout << "Fitnesses: " << endl;
        //for (Chromosome c : chromosomes)
            //cout << c.fitness << endl;
        if (generation++ >= max_generation)
            break;
    }
    
    // result is the maximum of the chromosomes vector
    long long res = INT_MIN;
    int resin;
    for (int i = 0; i < chromosomes.size(); i++)
    {
        if (chromosomes[i].fitness > res)
        {
            res = chromosomes[i].fitness;
            resin = i;
        }
    }

    planned_trains(chromosomes[resin]);
    long long max_ = 0;
    for (Gene g : chromosomes[resin].genes)
    {
        if (g.train_id != "")
        {
            max_ += g.Prize();
        }
    }

    cout << "Max: " << max_ << endl;

    return 0;
}