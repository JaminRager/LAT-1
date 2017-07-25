// MJD Data Set livetime & exposure calculator.
// C. Wiseman, USC
// S. Meijer and A. Reine, UNC

#include <iostream>
#include <map>
#include <ctime>
#include <sstream>
#include <iterator>
#include <array>
#include "TFile.h"
#include "TChain.h"
#include "TTreeReader.h"
#include "TTreeReaderArray.h"
#include "MJVetoEvent.hh"
#include "MJTRun.hh"
#include "GATDataSet.hh"
#include "GATChannelSelectionInfo.hh"
#include "GATDetInfoProcessor.hh"
#include "DataSetInfo.hh"

/*
  Results, 13 June 2017 (DataSetInfo.hh from this date.  db2 method used.)
  DS    Livetime   Enr. Act. Mass  Enr. Exp.      Nat. Act. Mass  Nat. Exp.
  DS0   46.0368    10.104          478.915        3.905           185.092
  DS1   58.93      11.3102         661.523        1.121           66.0583
  DS2   9.65746    11.3102         109.224        1.121           10.8256
  DS3   29.9112    12.0402         360.124        2.781           83.18
  DS4   23.6842    5.4712          129.578        3.95            93.5506
  DS5M1 121.779    12.0402         1436.07        3.912           452.974
  DS5M2 121.779    6.152           693.184        5.085           548.364
*/

using namespace std;
using namespace MJDB;

void calculateLiveTime(vector<int> runList, int dsNum, bool raw, bool runDB, bool noDT,
  map<int,vector<string>> ranges = map<int,vector<string>>(),
  vector<pair<int,double>> times = vector<pair<int,double>>(),
  map<int,vector<int>> burst = map<int,vector<int>>());

bool compareInterval(pair<int,int> i1, pair<int,int> i2) { return (i1.first < i2.first); }
int mergeIntervals(vector<pair<int,int>> vals, int start, int stop);
map<int,vector<int>> LoadBurstCut();
void getDBRunList(int &dsNum, double &ElapsedTime, string options, vector<int> &runList, vector<pair<int,double>> &times);
void locateRunRange(int run, map<int,vector<string>> ranges, int& runInSet, string& dtFilePath, bool& noDT);
map<int, vector<string>> getDeadtimeMap(int dsNum, bool& noDT, int dsNum_hi=-1) ;
double getTotalLivetimeUncertainty(map<int, double> livetimes, string opt="");
double getLivetimeAverage(map<int, double> livetimes, string opt="");
double getVectorUncertainty(vector<double> aVector);
double getVectorAverage(vector<double> aVector);
vector<uint32_t> getBestIDs(vector<uint32_t> input);

int main(int argc, char** argv)
{
  // "let's get some (m)args"
  if (argc < 2) {
		cout << "Usage: ./ds_livetime [dsNum] [options]\n"
         << " Options:\n"
         << "   -raw: Only get raw duration\n"
         << "   -gds: Get livetime from GATDataSet\n"
         << "   -db1 ['options in quotes']: Get run list from runDB and quit\n"
         << "   -db2 ['options in quotes']: Do full LT calculation on a runDB list\n"
         << "   -low: GDS method + low energy run/channel selection list.\n"
         << " RunDB access (-db[12] option):\n"
         << "    partNum = P3LQK, P3KJR, P3LQG, etc.\n"
         << "    runRank = gold, silver, bronze, cal, etc.\n"
         << "    dataset = 0 thru 6\n"
         << "    Ex.1: ./ds_livetime 5 -db1 'runrank P3KJR silver' (note the single quote)\n"
         << "    Ex.2: ./ds_livetime 5 -db1 'dataset 3'\n";
		return 1;
	}
  int dsNum = stoi(argv[1]);
  string runDBOpt = "";
  bool raw=0, gds=0, lt=1, rdb=0, low=0, noDT=0;
  vector<string> opt(argv+1, argv+argc);
  for (size_t i = 0; i < opt.size(); i++) {
    if (opt[i] == "-raw") { raw=1; }
    if (opt[i] == "-gds") { gds=1; }
    if (opt[i] == "-db1") { lt=0; rdb=1; runDBOpt = opt[i+1]; }
    if (opt[i] == "-db2") { lt=1; rdb=1; runDBOpt = opt[i+1]; }
    if (opt[i] == "-low") { lt=0; low=1; }
  }

  // -- Primary livetime routine, using DataSetInfo run sequences (default, no extra args) --
  if (lt && !rdb) {
    vector<int> runList;
    GATDataSet ds;
    cout << "Scanning DS-" << dsNum << endl;
    for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);
    for (size_t i = 0; i < ds.GetNRuns(); i++) runList.push_back(ds.GetRunNumber(i));
    map<int, vector<string>> ranges = getDeadtimeMap(dsNum,noDT);

    // -- Main routine --
    calculateLiveTime(runList,dsNum,raw,rdb,noDT,ranges);
  }

  // -- Do SIMPLE GATDataSet method and quit (-gds) --
  if (gds) {
    cout << "Scanning DS-" << dsNum << " with GetRunTime...\n";
    GATDataSet gds;
    for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(gds, dsNum, rs);
    cout << Form("DS-%i total from GetRunTime: %.4f days.\n",dsNum,gds.GetRunTime()/1e9/86400);
    return 0;
  }

  // -- Do SIMPLE RunDB method and quit (-db1) --
  if (!lt && rdb) {
    double ElapsedTime;
    vector<int> runList;
    vector<pair<int,double>> times;
    getDBRunList(dsNum, ElapsedTime, runDBOpt, runList, times);
    cout << Form("DS-%i total from RunDB: %.4f days.\n",dsNum,ElapsedTime/86400);
    return 0;
  }

  // -- Do primary livetime routine using a run list from the RunDB (-db2) --
  if (lt && rdb) {
    double ElapsedTime=0;
    vector<int> runList;
    vector<pair<int,double>> times;
    getDBRunList(dsNum, ElapsedTime, runDBOpt, runList, times); // auto-detects dsNum
    map<int, vector<string>> ranges = getDeadtimeMap(0,noDT,5); // we don't know what DS we're in, so load them all.
    // -- Main routine --
    calculateLiveTime(runList,dsNum,raw,rdb,noDT,ranges,times);
  }

  // -- Do primary livetime with a low-energy run+channels 'burst' cut applied. --
  if (low) {
    vector<int> runList;
    GATDataSet ds;
    cout << "Scanning DS-" << dsNum << endl;
    for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);
    for (size_t i = 0; i < ds.GetNRuns(); i++) runList.push_back(ds.GetRunNumber(i));
    map<int, vector<string>> ranges = getDeadtimeMap(dsNum,noDT);
    map<int,vector<int>> burst = LoadBurstCut(); // (low-energy run+channel selection)

    // -- Main routine --
    vector<pair<int,double>> times; // dummy (empty)
    calculateLiveTime(runList,dsNum,raw,rdb,noDT,ranges,times,burst);
  }
}

// =======================================================================================
void calculateLiveTime(vector<int> runList, int dsNum, bool raw, bool runDB, bool noDT,
  map<int,vector<string>> ranges,
  vector<pair<int,double>> times,
  map<int,vector<int>> burst)
{
  // Do we have deadtime files for this dataset?
  if (noDT) cout << "No deadtime files available, will show only runtime-exposure...\n";

  // Do we have M1 and M2 enabled?
  bool mod1=0, mod2=0;
  if (dsNum==0 || dsNum==1 || dsNum==2 || dsNum==3) { mod1=1; mod2=0; }
  else if (dsNum == 4)  { mod1=0; mod2=1; }
  else if (dsNum == 5)  { mod1=1; mod2=1; }

  // Are we applying a burst cut?
  bool useBurst=0;
  if (burst.size() > 0) {
    cout << "Applying burst cut...\n";
    useBurst=1;
  }

  // Load LN fill times
  double loFill = 15*60, hiFill = 5*60; // veto window
  vector<double> lnFillTimes[2];
  if (mod1) LoadLNFillTimes1(lnFillTimes[0], dsNum);
  if (mod2) LoadLNFillTimes2(lnFillTimes[1], dsNum);

  // Load detector maps
  map<int,bool> detIDIsBad = LoadBadDetectorMap(dsNum);
  map<int,bool> detIDIsVetoOnly = LoadVetoDetectorMap(dsNum);
  map<int,double> actM4Det_g = LoadActiveMasses(dsNum);
  map<int,bool> detIsEnr = LoadEnrNatMap();


  // ====== Loop over runs ======
  double runTime=0, vetoRunTime=0, vetoDead=0, m1LNDead=0, m2LNDead=0;
  map <int,double> channelRuntime, channelLivetime, channelLivetimeBest;
  map <int,int> detChanToDetIDMap;
  map <int,vector<double>> livetimeMap, livetimeMapBest;
  map<string, vector<double>> dtMap;
  bool firstTimeInSubset=true;   // Allows us to only add in pulser deadtime once per subset
  bool firstTimeInSubset2=true;  // A flag for the "best"

  time_t prevStop=0;
  int prevSubSet=-1;
  for (size_t r = 0; r < runList.size(); r++)
  {
    int run = runList[r];
    // cout << "Scanning run " << run << endl;
    if (fmod(100*(double)r/runList.size(), 10.0) < 0.1)
      cout << 100*(double)r/runList.size() << " % done, run " << run << endl;

     // Load the deadtime file ONLY when the subset changes.
    int runInSet = -1;
    string dtFilePath;
    if (!noDT) locateRunRange(run,ranges,runInSet,dtFilePath,noDT);
    if (!noDT && (runInSet != prevSubSet))
    {
      cout << "Subset " << runInSet << ", loading DT file:" << dtFilePath << endl;
      firstTimeInSubset = true;
      firstTimeInSubset2 = true;

      ifstream dtFile(dtFilePath.c_str());
      if (!dtFile) {
        cout << "Couldn't find file: " << dtFilePath << endl;
        return;
      }
      for (int i = 0; i < 2; i++) dtFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

      string buffer;
      dtMap.clear();
      while (getline(dtFile, buffer))
      {
        int id, pos;
        double p1, p2, p3, p4, p5, p6;
        double hgFWHM, hgNeg, hgPos, hgDead;
        double lgFWHM, lgNeg, lgPos, lgDead;
        double orDead;
        string det;
        istringstream iss(buffer);
        iss >> id >> pos >> hgFWHM >> hgNeg >> hgPos >> hgDead
            >> lgFWHM >> lgNeg >> lgPos >> lgDead >> orDead
            >> det >> p1 >> p2 >> p3 >> p4 >> p5 >> p6;
        // cout << Form("%i %i %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %s %.0f %.0f %.0f %.0f %.0f %.0f\n" ,id,pos,hgFWHM,hgNeg,hgPos,hgDead,lgFWHM,lgNeg,lgPos,lgDead,orDead,det.c_str(),p1,p2,p3,p4,p5,p6);

        // Check if anything is nan.  We'll take it to mean 100% dead.
        // This is maximally conservative, as it could be 100% live.
        if(hgDead != hgDead) hgDead = 100.0;
        if(lgDead != lgDead) lgDead = 100.0;
        if(orDead != orDead) orDead = 100.0;

        // fill the deadtime map
        dtMap[det] = {hgDead,lgDead,orDead,p1,p2,p3};
      }

      // Save the subset number
      prevSubSet = runInSet;
    }

    // Load built file
    GATDataSet ds;
    string bltPath = ds.GetPathToRun(run,GATDataSet::kBuilt);
    TFile *bltFile = new TFile(bltPath.c_str());


    // Get the runtime for this run.
    // Cover a bunch of stupid edge cases.
    double start=0, stop=0, thisRunTime=0;
    time_t startUnix=0, stopUnix=0;
    if (runDB) {
      runTime += times[r].second;
      stop = (double)times[r].first;
      start = (double)times[r].first - (double)times[r].second;
      thisRunTime = (stop-start);
    }
    else {
      MJTRun *runInfo = (MJTRun*)bltFile->Get("run");
      start = runInfo->GetStartClockTime();
      stop = runInfo->GetStopClockTime();
      thisRunTime = (stop-start)/1e9;
      if(thisRunTime < 0) {
        printf("Error, the runtime is negative! %.1f  -  %.1f  = %.2f   \n",start,stop,thisRunTime);
        startUnix = runInfo->GetStartTime();
        stopUnix = runInfo->GetStopTime();
        start = startUnix;
        stop = stopUnix;
        thisRunTime = (stopUnix-startUnix);
        printf("Reverting to the unix timestamps (%.2f) for run %d \n",thisRunTime,run);
      }
      runTime += thisRunTime;

      // still need unix times for LN fill deadtime calculation
      startUnix = runInfo->GetStartTime();
      stopUnix = runInfo->GetStopTime();
      struct tm *tmStart, *tmStop;  // I dunno if this is the best way to check for bad start/stop vals
      tmStart = gmtime(&startUnix), tmStop = gmtime(&stopUnix);
      int yrStart = 1900+tmStart->tm_year, yrStop = 1900+tmStop->tm_year;
      if (yrStart < 2005 || yrStart > 2025 || yrStop < 2005 || yrStart > 2025) {
        cout << Form("Run %i has corrupted start/stop packets.  Start (yr%i) %li  Stop (yr %i) %li.  Continuing...\n", run,yrStart,startUnix,yrStop,stopUnix);
        continue;
      }
    }
    if (raw) {
      delete bltFile;
      continue;
    }


    // Get veto system livetime and deadtime.
    double vetoDeadRun=0;
    if (dsNum!=4)
    {
      string vetPath = ds.GetPathToRun(run,GATDataSet::kVeto);
      if (FILE *file = fopen(vetPath.c_str(), "r")) {
        fclose(file);

        // Load tree and find veto livetime
        TFile *vetFile = new TFile(vetPath.c_str());
        TTree *vetTree = (TTree*)vetFile->Get("vetoTree");
        TTreeReader vReader(vetTree);
        TTreeReaderValue<MJVetoEvent> vetoEventIn(vReader,"vetoEvent");
        TTreeReaderValue<Long64_t> vetoStart(vReader,"start");
        TTreeReaderValue<Long64_t> vetoStop(vReader,"stop");
        TTreeReaderValue<double> timeUncert(vReader,"timeUncert");
        TTreeReaderArray<int> CoinType(vReader,"CoinType");
        vReader.SetEntry(0);
        long long vStart=0, vStop=0;
        vStart = (*vetoStart);
        vStop = (*vetoStop);
        if (runDB) {
          vetoRunTime += times[r].second; // use duration from runDB
          vStart = start; // use same duration as built files (this is OK)
          vStop = stop;
        }
        else vetoRunTime += (double)(vStop-vStart);
        vReader.SetTree(vetTree);  // resets the reader

        // Find veto deadtime
        bool newRun=1;
        while(vReader.Next()) {
          int idx = vReader.GetCurrentEntry();
          if (idx!=0) newRun=0;
          MJVetoEvent veto = *vetoEventIn;
          int type=0;
          if (CoinType[0]) type=1;
          if (CoinType[1]) type=2; // overrides type 1 if both are true
          if ((time_t)vStart-prevStop > 10 && newRun) type = 3; // in case we missed a muon in a run gap
          if (type!=0) {
            if (veto.GetBadScaler()) *timeUncert = 8.0; // fix uncertainty for corrupted scalers
            vetoDeadRun += 1. + 2 * fabs(*timeUncert); // matches muVeto window in skim_mjd_data
            // cout << Form("Run %i  type %i  uncert %-6.4f  vetoDeadRun %-6.4f  vetoDead %-6.4f\n", run,type,*timeUncert,vetoDeadRun,vetoDead);
          }
        }
        delete vetFile;
      }
    }
    else if (dsNum==4) {
      vector<int> muRuns, muTypes;
      vector<double> muRunTStarts, muTimes, muUncert;
      LoadDS4MuonList(muRuns,muRunTStarts,muTimes,muTypes,muUncert);

      vector<int> idxThisRun;
      for (size_t i = 0; i < muRuns.size(); i++)
        if (muRuns[i]==run) idxThisRun.push_back(i);

      for (size_t i = 0; i < idxThisRun.size(); i++) {
        int muIdx = idxThisRun[i];
        vetoDeadRun += 4 + 4. * fabs(muUncert[muIdx]); // matches muVeto window in skim_mjd_data
        // cout << Form("Run %i  type %i  uncert %-6.4f  vetoDeadRun %-6.4f  vetoDead %-6.4f\n", run,muTypes[muIdx],muUncert[muIdx],vetoDeadRun,vetoDead);
      }
    }
    prevStop = stop;
    vetoDead += vetoDeadRun;


    // Calculate LN fill deadtime.
    int m1LNDeadRun=0, m2LNDeadRun=0;
    // Find all fills whose veto window falls within this run.
    vector<pair<int,int>> runFills[2];
    for (int mod = 0; mod < 2; mod++) {
      for (auto fill : lnFillTimes[mod])
        if ((fill > startUnix && fill < stopUnix) ||         // fill within this run
           (fill < startUnix && fill+hiFill > startUnix) ||  // < 5 mins before this run
           (fill > stopUnix && fill-loFill < stopUnix)) {    // < 15 mins after this run
           runFills[mod].push_back(make_pair(fill-loFill,fill+hiFill));
         }
    }
    if (mod1 && runFills[0].size() > 0) m1LNDeadRun = mergeIntervals(runFills[0],startUnix,stopUnix);
    if (mod2 && runFills[1].size() > 0) m2LNDeadRun = mergeIntervals(runFills[1],startUnix,stopUnix);
    m1LNDead += (double)m1LNDeadRun;
    m2LNDead += (double)m2LNDeadRun;
    // if (m1LNDeadRun > 0 || m2LNDeadRun > 0)
    //   cout << Form("Found LN fill, run %i:  M1 nFills %lu  run %i  total %.0f -- M2 nFills %lu  run %i  total %.0f\n", run,runFills[0].size(),m1LNDeadRun,m1LNDead,runFills[1].size(),m2LNDeadRun,m2LNDead);


    // Calculate EACH ENABLED DETECTOR's runtime and livetime for this run, IF IT'S "GOOD".
    // NOTES:
    // - For each run, get a list (vector) of enabled channels.
    //   Then use the DataSetInfo veto-only and bad lists to pop channels from the list.
    //   Then look for a GATChannelSelectionInfo file and pop any additional channels.
    //   We don't count the runtime OR livetime from detectors that are on these lists.
    // - If we're applying a burst cut for this run, remove the affected channels.
    // - If we don't have a deadtime file, report only the runtime.

    MJTChannelMap *chMap = (MJTChannelMap*)bltFile->Get("ChannelMap");
    MJTChannelSettings *chSet = (MJTChannelSettings*)bltFile->Get("ChannelSettings");
    vector<uint32_t> enabledIDs = chSet->GetEnabledIDList();

    vector<uint32_t>::iterator ite = enabledIDs.begin();
    while (ite != enabledIDs.end()){
      int enab = *ite;
      GATDetInfoProcessor gp;
      int detID = gp.GetDetIDFromName( chMap->GetString(enab, "kDetectorName") );
      detChanToDetIDMap[enab] = detID;
      if (detIDIsVetoOnly[detID]) { enabledIDs.erase(ite); continue; } // erase increments the iterator
      else if (detIDIsBad[detID]) { enabledIDs.erase(ite); continue; }
      else ++ite;
    }

    // Now try and load a channel selection object and pop any other bad detectors
    string chSelPath = Form("/global/projecta/projectdirs/majorana/users/jwmyslik/analysis/channelselection/DS%i/v_20170510-00001",dsNum);
    if (FILE *file = fopen(chSelPath.c_str(), "r")) {
      fclose(file);
      GATChannelSelectionInfo ch_select (chSelPath, run);
      vector<int> DetIDList = ch_select.GetDetIDList();
      for (size_t ich=0; ich < DetIDList.size(); ich++)
      {
        int detID = DetIDList[ich];
        pair<int,int> ch_pair = ch_select.GetChannelsFromDetID(detID);
        bool isVetoDet = (ch_select.GetDetIsVetoOnly(detID));
        bool isBadDet = (ch_select.GetDetIsBad(detID));

        // If the channel is flagged, remove it from the enabledIDs.
        if (isVetoDet || isBadDet) {
          uint32_t hgChan = (uint32_t)ch_pair.first;
          auto it = std::find(enabledIDs.begin(), enabledIDs.end(), hgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);

          uint32_t lgChan = (uint32_t)ch_pair.second;
          it = std::find(enabledIDs.begin(), enabledIDs.end(), lgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);
        }
      }
    }


    // Now apply the burst cut
    if (useBurst)
    {
      // make a list of channels to remove
      vector<int> killChs;

      // search for the dataset-wide channels to remove
      map<int,vector<int>>::iterator it = burst.find(dsNum);
      if(it != burst.end())
        for (auto ch : it->second)
          killChs.push_back(ch);

      // search for the run-specific channels to remove
      map<int,vector<int>>::iterator it2 = burst.find(run);
      if(it2 != burst.end())
        for (auto ch : it2->second)
          killChs.push_back(ch);

      // now remove the channels
      for (auto ch : killChs) {
        auto it3 = std::find(enabledIDs.begin(), enabledIDs.end(), ch);
        if (it3 != enabledIDs.end()) {
          enabledIDs.erase(it3);
          cout << Form("DS %i  run %i  removed %i . enabled: ",dsNum,run,ch);
        }
      }
      for (auto ch : enabledIDs) cout << ch << " ";
      cout << endl;
    }


    // ==== Finally, add to the total runtime and livetime of ONLY GOOD detectors. =====

    // Add to HG and LG runtimes + livetimes
    vector<uint32_t> bestIDs = getBestIDs(enabledIDs);
    for (auto ch : enabledIDs)
    {
      if (detChanToDetIDMap[ch] == -1) continue;  // don't include pulser monitors.

      // Runtime
      channelRuntime[ch] += thisRunTime; // creates new entry if one doesn't exist
      if (noDT) continue;

      double thisLiveTime=0;

      string pos = chMap->GetDetectorPos(ch);
      if (dtMap.find(pos) != dtMap.end())
      {
        double hgDead = dtMap[pos][0]/100.0; // value is in percent, divide by 100
        double lgDead = dtMap[pos][1]/100.0;

        // Convert any negative fraction into a 1% deadtime. (David Radford says that's a safe assumption)
        if (hgDead < 0) hgDead = 0.01;
        if (lgDead < 0) lgDead = 0.01;

        // The following assumes only DS2 uses presumming, and may not always be true
        // Takes out 62 or 100 us per pulser as deadtime.
        double hgPulsers = dtMap[pos][3];
        double lgPulsers = dtMap[pos][4];
        double hgPulserDT = hgPulsers * (dsNum==2 || dsNum==6 ? 100e-6 : 62e-6);
        double lgPulserDT = lgPulsers * (dsNum==2 || dsNum==6 ? 100e-6 : 62e-6);

        // Get livetime for this channel
        if (ch%2 == 0) thisLiveTime = thisRunTime * (1 - hgDead) - hgPulserDT*(firstTimeInSubset?1:0);
        if (ch%2 == 1) thisLiveTime = thisRunTime * (1 - lgDead) - lgPulserDT*(firstTimeInSubset?1:0);
      }
      else {
        cout << "Warning: Detector " << pos << " not found! Exiting ...\n";
        return;
      }
      channelLivetime[ch] += thisLiveTime;

      // LN reduction - depends on if channel is M1 or M2
      double thisLNDeadTime = 0;
      GATDetInfoProcessor gp;
      int detID = gp.GetDetIDFromName( chMap->GetString(ch, "kDetectorName") );
      if (CheckModule(detID)==1) thisLNDeadTime = m1LNDeadRun;
      if (CheckModule(detID)==1) thisLNDeadTime = m2LNDeadRun;
      channelLivetime[ch] -= thisLNDeadTime;
      thisLiveTime -= thisLNDeadTime;

      // Veto reduction - applies to all channels in BOTH modules.
      channelLivetime[ch] -= vetoDeadRun;
      thisLiveTime -= vetoDeadRun;

      // Used for averages and uncertainty
      livetimeMap[ch].push_back(thisLiveTime/thisRunTime);
      firstTimeInSubset = false;
    }

    // Add to "Best" Livetime:  One entry per detector (loops over 'best' channel list)
    for (auto ch : bestIDs)
    {
      if (detChanToDetIDMap[ch] == -1) continue;
      if (noDT) continue;

      double bestLiveTime = 0;
      string pos = chMap->GetDetectorPos(ch);
      if (dtMap.find(pos) != dtMap.end())
      {
        double orDead = dtMap[pos][2]/100.0;
        if (orDead < 0) orDead = 0.01;

        double orPulsers = dtMap[pos][5];
        double orPulserDT = orPulsers*(dsNum==2 || dsNum==6 ? 100e-6 : 62e-6);
        bestLiveTime = thisRunTime * (1 - orDead) - orPulserDT*(firstTimeInSubset2?1:0);
      }
      else {
        cout << "Warning: Detector " << pos << " not found! Exiting ...\n";
        return;
      }
      channelLivetimeBest[ch] += bestLiveTime;

      double thisLNDeadTime = 0;
      GATDetInfoProcessor gp;
      int detID = gp.GetDetIDFromName( chMap->GetString(ch, "kDetectorName") );
      if (CheckModule(detID)==1) thisLNDeadTime = m1LNDeadRun;
      if (CheckModule(detID)==2) thisLNDeadTime = m2LNDeadRun;
      channelLivetimeBest[ch] -= thisLNDeadTime;
      bestLiveTime -= thisLNDeadTime;

      channelLivetimeBest[ch] -= vetoDeadRun;
      bestLiveTime -= vetoDeadRun;

      livetimeMapBest[ch].push_back(bestLiveTime/thisRunTime);
      firstTimeInSubset2 = false;
    }

    // Done with this run.
    delete bltFile;
  } // End loop over runs.


  // ========================================================
  // === Calculate channel-by-channel exposure in kg-days ===
  // If we don't have deadtime information, just calculate runtime exposure.

  runTime = runTime/86400;  // 86400 seconds = 1 day
  vetoRunTime = vetoRunTime/86400;
  vetoDead = vetoDead/86400;
  m1LNDead = m1LNDead/86400;
  m2LNDead = m2LNDead/86400;
  for (auto &raw : channelRuntime) raw.second = raw.second/86400.0;
  for (auto &live : channelLivetime) live.second = live.second/86400.0;
  for (auto &live : channelLivetimeBest) live.second = live.second/86400.0;

  // Calculate 3 times: H-runtime, L-runtime, and "Best"-LIVEtime
  double bestLivetime=0, hRuntime=0, lRuntime=0;
  for (auto &live : channelLivetimeBest) bestLivetime += live.second;
  for (auto &live : channelRuntime) {
    if (live.first%2==0) hRuntime += live.second;
    if (live.first%2==1) lRuntime += live.second;
  }

  double m1EnrExp=0, m1NatExp=0, m2EnrExp=0, m2NatExp=0;
  double m1EnrActMass=0, m1NatActMass=0, m2EnrActMass=0, m2NatActMass=0;
  map <int,double> channelExposure;

  // If we don't have deadtime information, only report RUNTIME-based results
  map <int,double> chanResults;
  if (noDT) chanResults = channelRuntime;
  else chanResults = channelLivetime;
  for (auto &live : chanResults)
  {
    int chan = live.first;
    double livetime = live.second;
    int detID = detChanToDetIDMap[chan];
    double activeMass = actM4Det_g[detID]/1000;
    channelExposure[chan] = activeMass * livetime;

    // don't double count LG channels or include pulser monitors
    if (chan%2==1 || detID==-1) continue;

    if (CheckModule(detID)==1 && detIsEnr[detID]==1) {
      m1EnrExp += channelExposure[chan];
      m1EnrActMass += activeMass;
    }
    if (CheckModule(detID)==1 && detIsEnr[detID]==0) {
      m1NatExp += channelExposure[chan];
      m1NatActMass += activeMass;
    }
    if (CheckModule(detID)==2 && detIsEnr[detID]==1) {
      m2EnrExp += channelExposure[chan];
      m2EnrActMass += activeMass;
    }
    if (CheckModule(detID)==2 && detIsEnr[detID]==0) {
      m2NatExp += channelExposure[chan];
      m2NatActMass += activeMass;
    }
  }

  // Now calculate exposure again for the "best" gain scenario
  // The masses are the same as previous HG calc.
  // Only do this if we have deadtime information.
  double m1EnrExpBest=0, m1NatExpBest=0, m2EnrExpBest=0, m2NatExpBest=0;
  map <int,double> bestExposure;
  if (!noDT) {
    for (auto &live : channelLivetimeBest)
    {
      int chan = live.first;
      double livetime = live.second;
      int detID = detChanToDetIDMap[chan];
      double activeMass = actM4Det_g[detID]/1000;
      bestExposure[detID] = activeMass * livetime;
      if (detID == -1) continue;
      if (CheckModule(detID)==1 && detIsEnr[detID]==1) m1EnrExpBest += bestExposure[detID];
      if (CheckModule(detID)==1 && detIsEnr[detID]==0) m1NatExpBest += bestExposure[detID];
      if (CheckModule(detID)==2 && detIsEnr[detID]==1) m2EnrExpBest += bestExposure[detID];
      if (CheckModule(detID)==2 && detIsEnr[detID]==0) m2NatExpBest += bestExposure[detID];
    }
  }

  // ============ Print results by module. ============
  time_t t = time(0);   // get time now
  struct tm * now = localtime( & t );
  cout << "\nDS " << dsNum << " Results, " << now->tm_year+1900 << "/" << now->tm_mon+1 << "/" << now->tm_mday << "\n"
       << "\tVeto Runtime " << vetoRunTime << "\n"
       << "\tVeto Deadtime " << vetoDead << "\n";

  if (noDT) cout << "Deadtime info not available -- reporting only runtime-based exposure ...\n";

  if (mod1) {
    cout << "Module 1:\n"
         << "\tRuntime : " << runTime << "\n"
         << "\tNumber of runs: " << runList.size() << " \n"
         << "\tLN Deadtime : " << m1LNDead << " (" << m1LNDead/runTime << ")\n"
         << "\tActive Enr Mass (kg): " << m1EnrActMass << "  Active Nat Mass: " << m1NatActMass << "\n";
    if (!noDT) {
      // cout << "\tBest Livetime (days): " << bestLivetime << "\n"
      cout << "\tBest Enr Exposure : " << m1EnrExpBest << "\n"
           << "\tBest Nat Exposure : " << m1NatExpBest << "\n";
    }
    else {
      cout << "\tHG Runtime (days) : " << hRuntime << " (used for exposure), LG Runtime : " << lRuntime << "\n"
           << "\tEnr Exposure : " << m1EnrExp << "\n"
           << "\tNat Exposure : " << m1NatExp << "\n";
    }
  }
  if (mod2) {
    cout << "Module 2:\n"
         << "\tRuntime : " << runTime << "\n"
         << "\tNumber of runs: " << runList.size() << " \n"
         << "\tLN Deadtime : " << m2LNDead << " (" << m2LNDead/runTime << ")\n"
         << "\tActive Enr Mass (kg): " << m2EnrActMass << "  Active Nat Mass: " << m2NatActMass << "\n";
    if (!noDT) {
      // cout << "\tBest Livetime (days): " << bestLivetime << "\n"
      cout << "\tBest Enr Exposure : " << m2EnrExpBest << "\n"
           << "\tBest Nat Exposure : " << m2NatExpBest << "\n";
    }
    else {
      cout << "\tHG Runtime (days) : " << hRuntime << " (used for exposure), LG Runtime : " << lRuntime << "\n"
           << "\tEnr Exposure : " << m2EnrExp << "\n"
           << "\tNat Exposure : " << m2NatExp << "\n";
    }
  }
  // Finally, print a channel-by-channel summary
  if (!noDT)
  {
    cout << "\nDetector summary with best (H or L) gain and deadtime correction : \n"
         << "Chan  DetID     A.M.(kg)  Runtime(d)  Livetime(d)  LT-Expo(kg-d)  AvgLTFrac  AvgLTUnc NRuns\n";
    for(auto &live : channelLivetimeBest)
    {
      int chan = live.first;
      double chLive = live.second;
      int detID = detChanToDetIDMap[chan];
      if (detID == -1) continue; // don't print pulser monitor chans

      double activeMass = actM4Det_g[detID]/1000;
      double ltAvg = getVectorAverage(livetimeMapBest[chan]);
      double ltUnc = getVectorUncertainty(livetimeMapBest[chan]);

      cout << Form("%-4i  %-8i  %-8.3f  %-10.4f  %-11.4f  %-13.4f  %-9.5f  %.5f  %d\n", chan,detID,activeMass,channelRuntime[chan],chLive,bestExposure[detID],ltAvg,ltUnc,livetimeMapBest[chan].size());
    }

    // Now report some average values for "all", "best", HG, and LG channel sets
    double chAllLTAvg = getLivetimeAverage(channelLivetime);
    double chBestLTAvg = getLivetimeAverage(channelLivetimeBest);
    double chHighLTAvg = getLivetimeAverage(channelLivetime,"HG");
    double chLowLTAvg = getLivetimeAverage(channelLivetime,"LG");
    cout << Form("Channel livetime averages - All %.4f  Best %.4f  HG %.4f  LG %.4f\n",chAllLTAvg,chBestLTAvg,chHighLTAvg,chLowLTAvg);

    double chAllLTUnc = getTotalLivetimeUncertainty(channelLivetime);
    double chBestLTUnc = getTotalLivetimeUncertainty(channelLivetimeBest);
    double chHighLTUnc = getTotalLivetimeUncertainty(channelLivetime,"HG");
    double chLowLTUnc = getTotalLivetimeUncertainty(channelLivetime,"LG");
    cout << Form("Channel livetime avg unc -  All %.4f  Best %.4f  HG %.4f  LG %.4f\n",chAllLTUnc,chBestLTUnc,chHighLTUnc,chLowLTUnc);

    vector<double> allAvg;
    vector<double> allUnc;
    for (auto& live : channelLivetime){
      allAvg.push_back(getVectorAverage(livetimeMap[live.first]));
      allUnc.push_back(getVectorUncertainty(livetimeMap[live.first]));
    }
    cout << "Total average fractional livetime (all channels) : " << getVectorAverage(allAvg) << endl
         << "Total average fractional livetime uncertainty    : " << getVectorUncertainty(allAvg) << endl;
  }

  cout << "\nAll-channel summary (no deadtime corrections) : \n"
       << "Chan  DetID     A.M.(kg)  Runtime(d)  RT-Expo(kg-d)\n";

  for(auto &raw : channelRuntime) // Loop over raw, not reduced livetimes for now.
  {
    int chan = raw.first;
    double chRun = raw.second;
    int detID = detChanToDetIDMap[chan];
    if (detID==-1) continue; // don't print pulser monitor chans
    double activeMass = actM4Det_g[detID]/1000;

    cout << Form("%-4i  %-8i  %-8.3f  %-10.4f  %-13.4f\n", chan,detID,activeMass,chRun,channelExposure[chan]);
  }
}


int mergeIntervals(vector<pair<int,int>> vals, int start, int stop)
{
  // Start with a vector of start/stop intervals, and sort in order of increasing start times.
  sort(vals.begin(), vals.end(), compareInterval);

  // Start a `final` list of intervals.  Then loop over the inputs.
  // Compare each interval to the most recent one on the stack.
  // If the current interval doesn't overlap with the top one, push it to the stack.
  // If the current interval DOES overlap w/ the top one, update the end time of the top one.
  vector<pair<int,int>> merged = {vals[0]};
  for (size_t i = 1 ; i < vals.size(); i++)
  {
    pair<int,int> top = merged.back();

    if (top.second < vals[i].first)
      merged.push_back(vals[i]);

    else if (top.second < vals[i].second) {
      top.second = vals[i].second;
      merged.pop_back();
      merged.push_back(top);
    }
  }

  // Now that we're merged, apply a global cutoff (run boundary).
  vector<pair<int,int>>::iterator it = merged.begin();
  while (it != merged.end()){
    pair<int,int> ival = *it;
    if (ival.first < start)  { ival.first = start; *it=ival; }
    if (ival.second > stop)  { ival.second = stop; *it=ival;}
    if (ival.second < start || ival.first > stop) { merged.erase(it); } // erase increments the iterator
    else it++;
  }

  // cout << "Result.  Start " << start << " ";
  // for (auto ival : merged) cout << "[" << ival.first << "," << ival.second << "] ";
  // cout << stop << " stop" << endl;

  // Finally, sum the intervals to get the total time.
  int deadTime = 0;
  for (auto ival : merged) deadTime += ival.second - ival.first;
  return deadTime;
}


// Used to pass multiple options to the DB as a single string.
// http://stackoverflow.com/questions/236129/split-a-string-in-c
template<typename Out>
void split(const string &s, char delim, Out result) {
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim)) *(result++) = item;
}
vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, back_inserter(elems));
    return elems;
}
void getDBRunList(int &dsNum, double &ElapsedTime, string options, vector<int> &runList, vector<pair<int,double>> &times)
{
  bool docs=true;

  // Parse the option string
  vector<string> opts = split(options,' '); // note the single quote
  string view = opts[0];
  string fullView = "";
  if (view == "runrank")
    fullView = Form("run_rank?key=[\"%s\",\"%s\"]",opts[1].c_str(),opts[2].c_str());
  else if (view == "dataset")
    fullView = Form("dataset?key=\"%i\"",stoi(opts[1]));

  cout << "view: " << view << endl
       << "fullview: " << fullView << endl;

  // Access DB
  const string dbString = "mjd_run_database";
  const string dbServer = "mjdb.phy.ornl.gov";
  MJDatabase runDB(&dbString, &dbServer);
  runDB.SetServerScheme("https");
  MJDocument runDoc;
  runDoc.Get_View(runDB,"dbApp",fullView,docs);
  string errorMessage;
  if (runDB.GetDBStatus(errorMessage)!=0){
    cout << "Failed to get document.  cURL error: " << runDB.GetDBStatus(errorMessage)
         << " Message: " << errorMessage << endl;
    return;
  }
  int nDocs = runDoc["rows"].Length();
  cout << "Found " << nDocs << " run records.\n";
  cout << runDB.GetURL() << endl; // you can check this in a browser
  // runDoc.Dump();

  // Loop over the document
  for (int i = 0; i < nDocs; i++)
  {
    int run = atoi(runDoc["rows"][i]["value"].Value().AsString().c_str());
    runList.push_back(run);

    // runDoc["rows"][i].Dump(); // dump just one document
    // int runInDoc = atoi(runDoc["rows"][i]["doc"]["RunNumber"].Value().AsString().c_str());
    // bool isOpen = ( runDoc["rows"][i]["doc"]["access"].Value().AsString() == "open" );
    // int runBits = atoi( runDoc["rows"][i]["doc"]["orca_run_bits"].Value().AsString().c_str() );
    // int runQuality = atoi( runDoc["rows"][i]["doc"]["RunQualityVal"].Value().AsString().c_str() );
    // string runRank = runDoc["rows"][i]["doc"]["RunRank"].Value().AsString();  // gold, silver, etc
    // string timestamp = runDoc["rows"][i]["doc"]["timestamp"].Value().AsString();
    int stopTime = atoi( runDoc["rows"][i]["doc"]["time"].Value().AsString().c_str() );
    double runElapsedTime = stod( runDoc["rows"][i]["doc"]["ElapsedTime"].Value().AsString() );
    // elapsedTimes.push_back(runElapsedTime);
    ElapsedTime += runElapsedTime;

    pair<int,double> tmp = make_pair(stopTime,runElapsedTime);
    times.push_back(tmp);

  }

  // Figure out the dataset
  dsNum = FindDataSet(runList[0]);
}


// Used to perform low-energy run selection.
map<int,vector<int>> LoadBurstCut()
{
  map<int, vector<int>> burst;
  ifstream inFile;
  inFile.open("burstCut_v1.txt");
  string line;
  while (getline(inFile, line)) {
    vector<string> inputs;
    istringstream iss(line);
    string buf;
    while (iss >> buf) inputs.push_back(buf);
    int key = stoi(inputs[0]);
    vector<int> cut;
    for (size_t i = 1; i < inputs.size(); i++) cut.push_back( stoi(inputs[i]) );
    burst[ key ] = cut;
  }
  // Also note channels that were entirely removed from the dataset.
  burst[0] = {656};
  burst[3] = {592,692};
  burst[4] = {1332};
  burst[5] = {614,692,1124,1232};
  return burst;
}


// Looks up which sub-range a particular run is in, given a range map.
void locateRunRange(int run, map<int,vector<string>> ranges, int& runInSet, string& dtFilePath, bool& noDT)
{
  bool foundRun = false;
  for (auto& r : ranges) {
    vector<string> thisRange = r.second; // c++ map trick: the first string is the file path, the rest are the run ranges.
    dtFilePath = thisRange[0];

    for (size_t i = 1; i < thisRange.size(); i+=2) {
      if (run >= stoi(thisRange[i]) && run <= stoi(thisRange[i+1])) {
        // cout << Form("Found run %i between runs %i and %i\n",run,stoi(thisRange[i]),stoi(thisRange[i+1]));
        foundRun = true;
        runInSet = r.first;
        break;
      }
    }
    if (foundRun) break;
  }
  if (!foundRun) {
    if (!noDT) cout << "Couldn't find deadtime file for run " << run << ". Reverting to runtime-only calculation ...\n";
    runInSet = -1;
    noDT = true;
  }
}

// Parses the 'lis' files in ./deadtime/ to make a range map.
// The first string is the file path, the rest are the 'int' run ranges.
map<int, vector<string>> getDeadtimeMap(int dsNum, bool& noDT, int dsNum_hi)
{
  map<int, vector<string>> ranges;

  // Load ranges for one ds or a range of DS's.
  vector<int> dsList;
  if (dsNum_hi == -1) dsList = {dsNum};
  else
    for (int i = dsNum; i <= dsNum_hi; i++)
      dsList.push_back(i);

  int rangeCount = 0;
  for (auto ds : dsList)
  {
    // Find runlist files for this dataset
    string command = Form("ls ./deadtime/ds%i_*.lis",ds);
    if (ds==5) command = Form("ls ./deadtime/DS*.lis");
    array<char, 128> buffer;
    vector<string> files;
    string str;
    shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) throw runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
      if (fgets(buffer.data(), 128, pipe.get()) != NULL) {
        str = buffer.data();
        str.erase(remove(str.begin(), str.end(), '\n'), str.end());  // strip newline
        files.push_back(str);
      }
    }

    // If we didn't find any files, set noDT = true and return.
    if (files.size()==0) {
      noDT=1;
      return ranges;
    }

    // Build the ranges.  Quit at the first sign of trouble.
    for (auto file : files)
    {
      // cout << file << endl;
      ifstream lisFile(file.c_str());
      if (!lisFile) {
        cout << "Couldn't find file: " << file << endl;
        return ranges;
      }
      string buffer;
      int firstRun = -1, lastRun = -1;
      while (getline(lisFile, buffer))
      {
        size_t found = buffer.find("Run");
        if (found == string::npos) {
          cout << "Couldn't find a run expression in " << buffer << endl;
          return ranges;
        }
        int run = stoi( buffer.substr(found+3) );
        if (firstRun == -1) firstRun = run;
        lastRun = run;
      }

      // grab the corresponding DT file
      string dtFile = file.substr(0, file.find_last_of(".")) + ".DT";

      // Fill the range map
      ranges[rangeCount] = {dtFile,to_string(firstRun),to_string(lastRun)};
      rangeCount++;
    }
  }
  return ranges;
}

double getTotalLivetimeUncertainty(map<int, double> livetimes, string opt)
{
  double sum_x = 0;
  double sum_x2 = 0;
  int n = 0;
  for (auto &values : livetimes)
  {
    if (opt=="HG" && values.first%2==0) {
      sum_x += values.second;
      sum_x2 += values.second*values.second;
      n++;
    }
    else if (opt=="LG" && values.first%2==1) {
      sum_x += values.second;
      sum_x2 += values.second*values.second;
      n++;
    }
    else {
      sum_x += values.second;
      sum_x2 += values.second*values.second;
      n++;
    }
  }
  double mean = sum_x / n;
  double stdev = sqrt((sum_x2 / n) - (mean * mean));
  return stdev / sqrt(n) ;
}

double getLivetimeAverage(map<int, double> livetimes, string opt)
{
  double sum_x = 0;
  int n = 0;

  for (auto &values : livetimes) {
    if (opt=="HG" && values.first%2==0) {
      sum_x += values.second;
      n++;
    }
    else if (opt=="HG" && values.first%2==1) {
      sum_x += values.second;
      n++;
    }
    else {
      sum_x += values.second;
      n++;
    }
  }
  return sum_x / n;
}

double getVectorUncertainty(vector<double> aVector)
{
  double sum_x = 0;
  double sum_x2 = 0;
  int n = aVector.size();

  for (int i=0; i<n; i++)
  {
    sum_x += aVector[i];
    sum_x2 += aVector[i]*aVector[i];
  }
  double mean = sum_x / n;
  double stdev = sqrt((sum_x2 / n) - (mean * mean));
  return stdev / sqrt(n) ;

}

double getVectorAverage(vector<double> aVector)
{
  double sum_x = 0;
  int n = aVector.size();

  for (int i=0; i<n; i++)
  {
    sum_x += aVector[i];
  }
  return sum_x / n;
}

vector<uint32_t> getBestIDs(vector<uint32_t> input)
{
  // This looks inside the input vector (probably enabledIDs) and finds:
  //    If a HG channel exists for a detector, it will add that in
  //    If only a LG channel exists, we add that in instead.
  // This should make sure that we're using as many detectors as we can

  vector<uint32_t> goodIDs;
  int n = input.size();

  for(int i=0; i<n; i++)
  {
    uint32_t aChannel = input[i];

    if(aChannel%2==1) // if it is LG
    {
      // check if HG is in input
      // if yes, it gets added later, if no, add LG here
      if(std::find(input.begin(), input.end(), aChannel-1) == input.end())
      { // enter here if aChannel is nowhere in the input
        goodIDs.push_back(aChannel);
      }
    }
    else{ goodIDs.push_back(aChannel); }
  }
  return goodIDs;
}