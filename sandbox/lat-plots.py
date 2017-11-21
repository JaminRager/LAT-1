#!/usr/bin/env python
import sys, imp, glob, os
sys.argv.append("-b") # kill all interactive crap
ds = imp.load_source('DataSetInfo','../DataSetInfo.py')
wl = imp.load_source('waveLibs','../waveLibs.py')
import ROOT
from ROOT import gROOT, gStyle, gPad
from ROOT import TFile, TTree, TChain, TCanvas, TH1D, TH2D, TF1, TLegend, TLine, TGraph

from matplotlib import pyplot as plt
import seaborn as sns
sns.set(style='whitegrid', context='talk')

homePath = os.path.expanduser('~')
bgDir = homePath + "/project/bg-lat"
calDir = homePath + "/project/cal-lat"

def main(argv):

    gStyle.SetOptStat(0)
    gROOT.ProcessLine("gErrorIgnoreLevel = 3001;")

    # fitMu()
    # wfStd()
    # fitSlo()




def bkgHists():
    """ brian wrote this one """

    dsNum, modNum = 1, 1

    # build the channel list  (remove 692 and 1232 from DS5 for now.)
    chList = ds.GetGoodChanList(dsNum)
    if dsNum==5 and modNum==1:
        chList = [ch for ch in chList if ch < 1000 and ch!=692]
    if dsNum==5 and modNum==2:
        chList = [ch for ch in chList if ch > 1000 and ch!=1232]

    bins,lower,upper = 2500,0,250

    skimTree = ROOT.TChain("skimTree")
    skimTree.Add("~/project/bg-lat/latSkimDS%d_*.root" % dsNum)
    cuts = "gain==0 && trapENFCal>0.7 && mHL==1 && isGood && !muVeto && !(C==1&&isLNFill1) && !(C==2&&isLNFill2) && C!=0&&P!=0&&D!=0"

    skimCut = ROOT.TChain("skimTree")
    hList = []
    hCutList = []
    outFile = ROOT.TFile("./data/BkgHisto_DS%d.root"%(dsNum), "RECREATE")
    outFile.cd()
    hFullTotal = ROOT.TH1D("hFullTotal", "", bins,lower,upper)
    hfitSloTotal = ROOT.TH1D("hfitSloTotal", "", bins,lower,upper)
    for idx, ch in enumerate(chList):
        print "Drawing histograms for channel", ch
        hList.append(ROOT.TH1D())
        hCutList.append(ROOT.TH1D())
        skimCut.Reset()
        skimCut.Add("/projecta/projectdirs/majorana/users/wisecg/cuts/fs/fitSlo-DS%d-*-ch%d.root"%(dsNum, ch))
        hCutList[idx] = wl.H1D(skimCut,bins,lower,upper, "trapENFCalC", cuts+"&&channel==%d"%(ch), Title="hfitSlo_ch%d"%(ch))
        hList[idx] = wl.H1D(skimTree,bins,lower,upper,"trapENFCalC", cuts+"&&channel==%d"%(ch),Title="hFull_ch%d"%(ch))
        hfitSloTotal.Add(hCutList[idx])
        hFullTotal.Add(hList[idx])
        hCutList[idx].Write()
        hList[idx].Write()

    hfitSloTotal.Write()
    hFullTotal.Write()
    outFile.Close()


def fitMu():

    # load cal files the way job-panda does
    dsNum = 3
    # nIdx = wl.getNCalIdxs(dsNum, module=1)
    calIdx = 15
    fileList = wl.getCalFiles(dsNum, calIdx)
    lat = TChain("skimTree")
    for f in fileList: lat.Add(f)
    print "Found",lat.GetEntries(),"entries."

    h1 = wl.H2D(lat,50,0,50,500,-1000,25000,"fitMu:trapENFCalC","","Energy (keV)","fitMu","")
    h1.Draw("colz")
    c.SetLogz(1)
    c.Print("../plots/fitMu.pdf")


def fitSlo():
    """ make a raw spectrum + fitslo cut for each ds.
        rn we have DS1234.
    """

    dsNum = 1
    fsList = glob.glob("/global/homes/w/wisecg/project/cuts/fs/fitSlo-DS%d*" % dsNum)
    latList = glob.glob("/globa/homes/w/wisecg/project/bg-lat/latSkimDS%d" % dsNum)

    # chList = ds.GetGoodChanList(dsNum)
    # outFile = "/global/homes/w/wisecg/project/cuts/fs/fitSlo-DS%d-%d-ch%d.root" % (dsNum, subNum, ch)



if __name__=="__main__":
    main(sys.argv[1:])