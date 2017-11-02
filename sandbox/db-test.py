#!/usr/bin/env python
import imp, glob
ds = imp.load_source('DataSetInfo','../DataSetInfo.py')
wl = imp.load_source('waveLibs','../waveLibs.py')
import tinydb as db
from ROOT import TFile, TTree, TNamed, TChain
from ROOT import gROOT
"""
    DB Notes:

    tinyDB items are nested dicts:
    {"key":key, "vals":vals}

    Cal Tables (gives run coverages):
        key: ds[DS]_calIdx.
        vals: {[idx]:[cal lo, cal hi, cov lo, cov hi]}
        Print one with:
        wl.getDBCalTable(dsNum, verbose=True)

    Cal Records (calib consts for each channel in each calIdx)
        key: ds[DS]_idx[n]
        vals: {[chan]:[trapENF, fitAmp, latAF, latAFC]}
        - channel list comes from DataSetInfo.py

    Cut Records:
        key: [Name]_ds[i]_idx[j]_module[k]_[descriptor].
            Names are: "riseNoise", "fitSlo", "bcMax", "pol2", and "pol3"
            idx is the calIdx
            Descriptor is two numbers (energy range), "continuum", or "peak"
                Continuum range is 5-50 kev
                Peak is 236-240
            descriptors: Peak, Continuum, 50_90, 90_130, 130_170, 170_210
        vals: {[chan]:[1%, 5%, 90%, 95%, 99%]}
        - channel list comes from DataSetInfo.py
        - calIdx list comes from the CalInfo object in DataSetInfo.py, NOT the cal tables in the DB!!!
"""

def main():

    gROOT.ProcessLine("gErrorIgnoreLevel = 3001;") # suppress ROOT error messages

    # access cal idx's the way job-panda would have
    # fileList = getCalFiles(1, verbose=False)

    # applyDBCuts()

    # set up looping over all subsets
    # lat3 usage will be 1 job per dataset

    dsNum = 0 # lat3 should take dsNum as an argument
    nRanges = ds.dsMap[dsNum]
    nMods = [1]
    if dsNum == 4: nMods = [2]
    if dsNum == 5: nMods = [1,2]

    for modNum in nMods:

        for subNum in range(nRanges):

            fRegex = "/global/homes/w/wisecg/project/bg-lat/latSkimDS%d_%d_*.root" % (dsNum, subNum)
            fList = glob.glob(fRegex)
            file0 = fList[0]

            print "DS-%d Sub-%d M%d N: %d" % (dsNum, subNum, modNum, len(fList))

            applyDBCuts(dsNum, subNum, modNum, fRegex, file0)

            return


def applyDBCuts(dsNum, subNum, modNum, fRegex, file0):

    cInfo = ds.CalInfo()

    skimTree = TChain("skimTree")
    skimTree.Add(fRegex)
    f = TFile(file0)
    theCut = f.Get("theCut").GetTitle()
    chList = ds.GetGoodChanList(dsNum)

    megaCut = MakeCutList(cInfo, skimTree, theCut, dsNum, modNum, chList)
    for key in megaCut: print key, megaCut[key]

    # for idx, ch in enumerate(chList):
    #     chanCut = theCut+'&&'+megaCut[ch][2:]
    #     outFile = "/global/homes/w/wisecg/project/cuts/fs/fitSlo-DS%d-%d-ch%d.root" % (dsNum, subNum, ch)
    #     print "Writing to:",outFile
    #     print "Cut used:",chanCut

        # outFile = TFile(outFile,"RECREATE")
        # outTree = TTree()
        # outTree = skimTree.CopyTree(chanCut)
        # outTree.Write()
        # cutUsed = TNamed("chanCut",chanCut)
        # cutUsed.Write()
        # outFile.Close()


def MakeCutList(cInfo, skimTree, basicCut, dsNum, modNum, chList=[], mode='db'):
    """ Pass in background skim file and it generates a dictionary of cuts for all good channels"""

    # nPass = skimTree.Draw("run", basicCut, "goff")
    # nRun = skimTree.GetV1()
    # runList = list(set(int(nRun[n]) for n in xrange(nPass)))
    # print "Processing Runs", runList

    # find calIdx boundaries for these runs
    run = 0
    skimTree.SetBranchAddress("run",run)
    skimTree.GetEntry(0)



    idxMin = cInfo.GetCalIdx("ds%d_m%d"%(dsNum, modNum), runList[0])
    idxMax = cInfo.GetCalIdx("ds%d_m%d"%(dsNum, modNum), runList[-1])

    # build a big cut
    megaCut = {}
    for subNum in range(idxMin,idxMax+1):

        runMin, runMax = cInfo.master['ds%d_m%d'%(dsNum,modNum)][subNum][1], cInfo.master['ds%d_m%d'%(dsNum,modNum)][subNum][2]
        fsD = wl.getDBCalRecord("fitSlo_ds%d_idx%d_m%d_Peak" % (dsNum,subNum,modNum))

        for ch in chList:

            fsVal = fsD[ch][2] # 90% value

            # add an 'if' condition for zeros - those are bad cut records
            if fsVal > 0:
                if ch in megaCut.keys():
                    megaCut[ch] += "|| (run>=%d && run<=%d && fitSlo<%.2f)" % (runMin, runMax, fsVal)
                else:
                    megaCut[ch] = "|| (run>=%d && run<=%d && fitSlo<%.2f)" % (runMin, runMax, fsVal)

    return megaCut



def getCalFiles(dsNum, calIdx=None, verbose=False):
    """ Get a list of all files for a particular dsNum+calIdx. """

    calInfo = ds.CalInfo()
    calKeys = calInfo.GetKeys(dsNum)

    fList = []
    for key in calKeys:
        print key

        # number of cal subsets
        nIdx = calInfo.GetIdxs(key)

        # get the runs in each calIdx
        runList = []
        if calIdx!=None:
            runList = calInfo.GetCalList(key, calIdx, 10)
            if verbose: print runList
        else:
            for idx in range(nIdx):
                tmp = calInfo.GetCalList(key, idx, 10)
                if verbose: print tmp
                runList += tmp

        # make a list of the actual file paths
        for run in runList:
            fPath = "%s/latSkimDS%d_run%d*.root" % (calDir, dsNum, run)
            fList += glob.glob(fPath)

    # for f in fList: print f
    return fList


def testWLFunctions():

    # wl.setDBCalTable()
    # wl.getDBKeys()
    # wl.getDBCalRecord("ds1_idx0")
    # wl.getDBCalRecord("ds1_calIdx")
    # wl.getDBCalRecord("fitSlo_ds1_idx55_m1_170_210")
    # wl.delDBRecord("ds1_idx0")
    # wl.getDBCalTable(5)
    # wl.getDBRunCoverage(1,9999)

    # cal = ds.CalInfo()

    # look at what's in our database
    # calTab = wl.getDBCalTable(dsNum)
    # print len(calTab)  # 62

    # get a cal index for a run
    # key, run = "ds1_m1",10770
    # print cal.GetCalIdx(key,run)

    # generate a list of cal runs for a given index
    # key, idx = "ds3_m1",4
    # print cal.GetCalList(key,idx,runLimit=10)

    # generate cal runs for a given dataset
    # key = "ds2_m1"
    # for idx in range(cal.GetIdxs(key)):
        # print cal.GetCalList(key,idx,runLimit=10)

    # generate all possible cal runs
    # for key in cal.GetKeys():
        # print key
        # for idx in range(cal.GetIdxs(key)):
            # print cal.GetCalList(key,idx,runLimit=10)

    # example of iterating over the DB
    calDB = db.TinyDB('../calDB.json')
    for item in calDB:
        d = dict(item)
        key = d["key"]
        vals = d["vals"]
        tmp = key.split("_")
        tmp = [str(t) for t in tmp]

        if tmp[0]=="fitSlo" and tmp[1]=="ds%d" % dsNum:
            print tmp
            # print vals
            # return

        # nRec += 1
        # if nRec > 10: break


if __name__=="__main__":
    main()