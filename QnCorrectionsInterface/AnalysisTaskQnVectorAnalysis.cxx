/*
 ***********************************************************
 Manager for event plane corrections framework
Contact: Jaap Onderwaater, j.onderwaater@gsi.de, jacobus.onderwaater@cern.ch
Instructions in AddTask_EPcorrectionsExample.C
2014/12/10
 *********************************************************
 */

//#include "AliSysInfo.h"
#include <iostream>

#include <TROOT.h>
#include <TTimeStamp.h>
#include <TStopwatch.h>
#include <TChain.h>
#include <AliInputEventHandler.h>
#include <AliESDInputHandler.h>
#include <AliAODInputHandler.h>
#include <AliAnalysisManager.h>
#include <AliCentrality.h>
#include <AliESDEvent.h>
#include <TList.h>
#include <TGraphErrors.h>
#include <AliLog.h>
#include "QnCorrectionsCuts.h"
#include "QnCorrectionsManager.h"
#include "AliQnCorrectionsHistos.h"
#include "AnalysisTaskQnVectorAnalysis.h"
#include "QnCorrectionsQnVector.h"

// make a change

using std::cout;
using std::endl;


ClassImp(AnalysisTaskQnVectorAnalysis)

/* names for the different TProfile */
TString namesQnTrackDetectors[AnalysisTaskQnVectorAnalysis::nTrackDetectors] = {"TPC","SPD"};
TString namesQnEPDetectors[AnalysisTaskQnVectorAnalysis::nEPDetectors] = {"V0A","V0C","FMDA","FMDC"};
TString namesQnComponents[AnalysisTaskQnVectorAnalysis::kNcorrelationComponents] = {"XX","XY","YX","YY"};

/* centrality binning */
const Int_t nQnCentBins = 11;
Double_t centQnBinning[nQnCentBins+1] = {0.0, 5.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0};
Double_t centQnBinningmid[nQnCentBins] = {2.5, 7.5, 15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0, 95.0};

//_________________________________________________________________________________
AnalysisTaskQnVectorAnalysis::AnalysisTaskQnVectorAnalysis() :
        QnCorrectionsFillEventTask(),
  fEventQAList(0x0),
  fEventCuts(NULL),
  fEventPlaneHistos(0x0),
  fVn(),
  fNDetectorResolutions(0),
  fDetectorResolution(),
  fDetectorResolutionContributors(),
  fDetectorResolutionCorrelations(),
  fTrackDetectorNameInFile(),
  fEPDetectorNameInFile()
{
  //
  // Default constructor
  //
}

//_________________________________________________________________________________
AnalysisTaskQnVectorAnalysis::AnalysisTaskQnVectorAnalysis(const char* name) :
        QnCorrectionsFillEventTask(name),
  fEventQAList(0x0),
  fEventCuts(0x0),
  fEventPlaneHistos(0x0),
  fVn(),
  fNDetectorResolutions(0),
  fDetectorResolution(),
  fDetectorResolutionContributors(),
  fDetectorResolutionCorrelations(),
  fTrackDetectorNameInFile(),
  fEPDetectorNameInFile()
{
  //
  // Constructor
  //

  fEventQAList = new TList();
  fEventQAList->SetName("EventQA");
  fEventQAList->SetOwner(kTRUE);

  fEventPlaneHistos = new AliQnCorrectionsHistos();

  /* initialize TProfile storage */
  for(Int_t i=0; i < nTrackDetectors*nEPDetectors; i++) {
    for(Int_t j=0; j < kNharmonics; j++) {
      for(Int_t k=0; k < kNcorrelationComponents; k++) {
        fVn[i][j][k]=NULL;
      }
    }
  }

  for (Int_t i=0; i < nTrackDetectors*nEPDetectors; i++) {
    for (Int_t j = 0; j < kNresolutionComponents; j++) {
      fDetectorResolutionContributors[i][j] = -1;
    }
    for (Int_t iCorr = 0; iCorr < nCorrelationPerDetector; iCorr++) {
      for(Int_t h=0; h < kNharmonics; h++) {
        fDetectorResolutionCorrelations[i][iCorr][h] = NULL;
      }
    }
    for(Int_t h=0; h < kNharmonics; h++) {
      fDetectorResolution[i][h] = NULL;
    }
  }

  /* the index in the input TList structure for the data of the different detectors */
  fTrackDetectorNameInFile[kTPC] = "TPC";
  fTrackDetectorNameInFile[kSPD] = "SPD";
  fEPDetectorNameInFile[kVZEROA] = "VZEROA";
  fEPDetectorNameInFile[kVZEROC] = "VZEROC";
  fEPDetectorNameInFile[kFMDA] = "FMDAraw";
  fEPDetectorNameInFile[kFMDC] = "FMDCraw";

  /* the detector resolution configurations */
  fNDetectorResolutions = 4;
  /* this is only valid in C++11
  fDetectorResolutionContributors = {
      {kTPC,kVZEROC,kVZEROA},
      {kTPC,kFMDA,kFMDC},
      {kSPD,kVZEROC,kVZEROA},
      {kSPD,kFMDA,kFMDC}
  }; */
  /* for the time being we do it in this awful way */
  Int_t config0[kNresolutionComponents] = {kTPC,kVZEROC,kVZEROA};
  Int_t config1[kNresolutionComponents] = {kTPC,kFMDA,kFMDC};
  Int_t config2[kNresolutionComponents] = {kSPD,kVZEROC,kVZEROA};
  Int_t config3[kNresolutionComponents] = {kSPD,kFMDA,kFMDC};
  for (Int_t i = 0; i < kNresolutionComponents; i++) {
    fDetectorResolutionContributors[0][i] = config0[i];
    fDetectorResolutionContributors[1][i] = config1[i];
    fDetectorResolutionContributors[2][i] = config2[i];
    fDetectorResolutionContributors[3][i] = config3[i];
  }

  /* create the needed TProfile for each of the track-EP detector combination
   * and for each of the v_n and for the different correlation namesQnComponents */
  for(Int_t iTrkDetector=0; iTrkDetector < nTrackDetectors; iTrkDetector++) {
    for (Int_t iEPDetector=0; iEPDetector < nEPDetectors; iEPDetector++) {
      for(Int_t h=0; h<kNharmonics; h++) {
        for(Int_t corrComp =0; corrComp< kNcorrelationComponents; corrComp++){
          fVn[iTrkDetector*nEPDetectors+iEPDetector][h][corrComp] =
              new TProfile(Form("vn_%sx%s_%s_h%d", namesQnTrackDetectors[iTrkDetector].Data(), namesQnEPDetectors[iEPDetector].Data(), namesQnComponents[corrComp].Data(),h+1),
                  Form("vn_%sx%s_%s_h%d", namesQnTrackDetectors[iTrkDetector].Data(), namesQnEPDetectors[iEPDetector].Data(), namesQnComponents[corrComp].Data(),h+1),
                  nQnCentBins,
                  centQnBinning);
        }
      }
    }
  }

  /* create the needed correlation TProfile for each the detector resolution and desired additional detector configuration */
  for(Int_t ixDetectorConfig = 0; ixDetectorConfig < nTrackDetectors*nEPDetectors; ixDetectorConfig++) {
    /* filter out not initialized detector configurations */
    if(fDetectorResolutionContributors[ixDetectorConfig][0]==-1) continue;
    for (Int_t iCorr = 0; iCorr < nCorrelationPerDetector; iCorr++) {
      TString detectorOneName;
      TString detectorTwoName;
      TString correlationComponent;
      switch (iCorr) {
      case kABXX: {
        detectorOneName = namesQnTrackDetectors[fDetectorResolutionContributors[ixDetectorConfig][0]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][1]];
        correlationComponent = namesQnComponents[kXX];
        break;
      }
      case kABYY: {
        detectorOneName = namesQnTrackDetectors[fDetectorResolutionContributors[ixDetectorConfig][0]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][1]];
        correlationComponent = namesQnComponents[kYY];
        break;
      }
      case kACXX: {
        detectorOneName = namesQnTrackDetectors[fDetectorResolutionContributors[ixDetectorConfig][0]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][2]];
        correlationComponent = namesQnComponents[kXX];
        break;
      }
      case kACYY: {
        detectorOneName = namesQnTrackDetectors[fDetectorResolutionContributors[ixDetectorConfig][0]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][2]];
        correlationComponent = namesQnComponents[kYY];
        break;
      }
      case kBCXX: {
        detectorOneName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][1]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][2]];
        correlationComponent = namesQnComponents[kXX];
        break;
      }
      case kBCYY: {
        detectorOneName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][1]];
        detectorTwoName = namesQnEPDetectors[fDetectorResolutionContributors[ixDetectorConfig][2]];
        correlationComponent = namesQnComponents[kYY];
        break;
      }
      }
      for(Int_t h=0; h < kNharmonics; h++) {
        TString profileName = Form("corr_%sx%s_%s_h%d",
            detectorOneName.Data(),
            detectorTwoName.Data(),
            correlationComponent.Data(),
            h+1);
        TString newProfileName = Form("new_corr_%sx%s_%s_h%d",
            detectorOneName.Data(),
            detectorTwoName.Data(),
            correlationComponent.Data(),
            h+1);
        fDetectorResolutionCorrelations[ixDetectorConfig][iCorr][h] = new TProfile(profileName.Data(),profileName.Data(), nQnCentBins, centQnBinning);
      }
    }
  }

  /* create the sub-event structures to store the cross correlations */

  DefineInput(0,TChain::Class());
  DefineInput(1,TList::Class());
  DefineOutput(1, TList::Class());// Event QA histograms
}

AnalysisTaskQnVectorAnalysis::~AnalysisTaskQnVectorAnalysis() {
  /* clean up everything before leaving */
  for(Int_t iTrkDetector=0; iTrkDetector < nTrackDetectors; iTrkDetector++) {
    for (Int_t iEPDetector=0; iEPDetector < nEPDetectors; iEPDetector++) {
      for(Int_t h=0; h<kNharmonics; h++) {
        for(Int_t corrComp =0; corrComp< kNcorrelationComponents; corrComp++){
          delete fVn[iTrkDetector*nEPDetectors+iEPDetector][h][corrComp];
        }
      }
    }
  }
  for(Int_t ixDetectorConfig = 0; ixDetectorConfig < nTrackDetectors*nEPDetectors; ixDetectorConfig++) {
    /* filter out not initialized detector configurations */
    if(fDetectorResolutionContributors[ixDetectorConfig][0]==-1) continue;
    for (Int_t iCorr = 0; iCorr < nCorrelationPerDetector; iCorr++) {
      for(Int_t h=0; h < kNharmonics; h++) {
        delete fDetectorResolutionCorrelations[ixDetectorConfig][iCorr][h];
      }
    }
    for(Int_t h=0; h < kNharmonics; h++) {
      // delete fDetectorResolution[ixDetectorConfig][h];
      // delete fNewDetectorResolution[ixDetectorConfig][h];
    }
  }
}
//_________________________________________________________________________________
void AnalysisTaskQnVectorAnalysis::UserCreateOutputObjects()
{
  //
  // Add all histogram manager histogram lists to the output TList
  //

  PostData(1, fEventQAList);

}



//________________________________________________________________________________________________________
void AnalysisTaskQnVectorAnalysis::UserExec(Option_t *){
  //
  // Main loop. Called for every event
  //

#define MAXNOOFDATAVARIABLES 2048

  fEvent = InputEvent();

  /* Get the Qn vectors list */
  TList* qnlist = dynamic_cast<TList*>(GetInputData(1));
  if(!qnlist) return;

  Float_t values[MAXNOOFDATAVARIABLES] = {-999.};
  fDataBank = values;

  FillEventData();

  fEventPlaneHistos->FillHistClass("Event_NoCuts", values);
  if(!IsEventSelected(values)) return;
  fEventPlaneHistos->FillHistClass("Event_Analysis", values);

  TList* newTrkQvecList[nTrackDetectors] = {NULL};
  TList* newEPQvecList[nEPDetectors] = {NULL};
  QnCorrectionsQnVector* newTrk_qvec[nTrackDetectors] = {NULL};
  QnCorrectionsQnVector* newEP_qvec[nEPDetectors] = {NULL};


  /* get data structures for the different track detectors */
  const char *szCorrectionPass = "align";
  const char *szAltCorrectionPass = "rec";
  for (Int_t iTrk = 0; iTrk < nTrackDetectors; iTrk++) {
    newTrkQvecList[iTrk] = dynamic_cast<TList*> (qnlist->FindObject(Form("%s", fTrackDetectorNameInFile[iTrk].Data())));
    newTrk_qvec[iTrk] = (QnCorrectionsQnVector*) newTrkQvecList[iTrk]->FindObject(szCorrectionPass);
    if (newTrk_qvec[iTrk] == NULL) {
      newTrk_qvec[iTrk] = (QnCorrectionsQnVector*) newTrkQvecList[iTrk]->FindObject(szAltCorrectionPass);
      if (newTrk_qvec[iTrk] == NULL) return; /* neither the expected nor the alternative were there */
    }
  }

  /* and now for the EP detectors */
  for (Int_t iEP = 0; iEP < nEPDetectors; iEP++) {
    newEPQvecList[iEP] = dynamic_cast<TList*> (qnlist->FindObject(Form("%s",fEPDetectorNameInFile[iEP].Data())));
    newEP_qvec[iEP] = (QnCorrectionsQnVector*) newEPQvecList[iEP]->FindObject(szCorrectionPass);
    if (newEP_qvec[iEP] == NULL) {
      newEP_qvec[iEP] = (QnCorrectionsQnVector*) newEPQvecList[iEP]->FindObject(szAltCorrectionPass);
      if (newEP_qvec[iEP] == NULL) return; /* neither the expected nor the alternative were there */
    }
  }

  /* now fill the Vn profiles with the proper data */
  /* TODO: correct for the normalization */
  for(Int_t iTrkDetector=0; iTrkDetector < nTrackDetectors; iTrkDetector++) {
    /* sanity check */
    if (newTrk_qvec[iTrkDetector]->IsGoodQuality()) {
      for (Int_t iEPDetector=0; iEPDetector < nEPDetectors; iEPDetector++) {
        /*sanity check */
        if (newEP_qvec[iEPDetector]->IsGoodQuality()) {
          for(Int_t h=0; h < kNharmonics; h++) {
            fVn[iTrkDetector*nEPDetectors+iEPDetector][h][0]->Fill(values[AliQnCorrectionsVarManager::kVZEROMultPercentile],
                    newTrk_qvec[iTrkDetector]->Qx(h+1) * newTrk_qvec[iTrkDetector]->GetN() * newEP_qvec[iEPDetector]->QxNorm(h+1));
            fVn[iTrkDetector*nEPDetectors+iEPDetector][h][1]->Fill(values[AliQnCorrectionsVarManager::kVZEROMultPercentile],
                    newTrk_qvec[iTrkDetector]->Qx(h+1) * newTrk_qvec[iTrkDetector]->GetN() * newEP_qvec[iEPDetector]->QyNorm(h+1));
            fVn[iTrkDetector*nEPDetectors+iEPDetector][h][2]->Fill(values[AliQnCorrectionsVarManager::kVZEROMultPercentile],
                    newTrk_qvec[iTrkDetector]->Qy(h+1) * newTrk_qvec[iTrkDetector]->GetN() * newEP_qvec[iEPDetector]->QxNorm(h+1));
            fVn[iTrkDetector*nEPDetectors+iEPDetector][h][3]->Fill(values[AliQnCorrectionsVarManager::kVZEROMultPercentile],
                    newTrk_qvec[iTrkDetector]->Qy(h+1) * newTrk_qvec[iTrkDetector]->GetN() * newEP_qvec[iEPDetector]->QyNorm(h+1));
          }
        }
      }
    }
  }

  /* now fill the correlation profiles needed for detector resolution */
  for (Int_t ix = 0; ix < fNDetectorResolutions; ix++) {
    /* sanity checks */
    if(fDetectorResolutionContributors[ix][0] == -1) continue;

    /* sanity checks */
    if((newTrk_qvec[fDetectorResolutionContributors[ix][0]] != NULL) &&
        (newEP_qvec[fDetectorResolutionContributors[ix][1]] != NULL) &&
        (newEP_qvec[fDetectorResolutionContributors[ix][2]] != NULL) &&
        (newTrk_qvec[fDetectorResolutionContributors[ix][0]]->GetN() != 0) &&
        (newEP_qvec[fDetectorResolutionContributors[ix][1]]->GetN() != 0) &&
        (newEP_qvec[fDetectorResolutionContributors[ix][2]]->GetN() != 0)) {

      for(Int_t h=0; h < kNharmonics; h++) {
        for (Int_t iCorr = 0; iCorr < nCorrelationPerDetector; iCorr++) {
          Float_t detectorOneValue;
          Float_t detectorTwoValue;
          switch (iCorr) {
          case kABXX: {
            detectorOneValue = newTrk_qvec[fDetectorResolutionContributors[ix][0]]->QxNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][1]]->QxNorm(h+1);
            break;
          }
          case kABYY: {
            detectorOneValue = newTrk_qvec[fDetectorResolutionContributors[ix][0]]->QyNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][1]]->QyNorm(h+1);
            break;
          }
          case kACXX: {
            detectorOneValue = newTrk_qvec[fDetectorResolutionContributors[ix][0]]->QxNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][2]]->QxNorm(h+1);
            break;
          }
          case kACYY: {
            detectorOneValue = newTrk_qvec[fDetectorResolutionContributors[ix][0]]->QyNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][2]]->QyNorm(h+1);
            break;
          }
          case kBCXX: {
            detectorOneValue = newEP_qvec[fDetectorResolutionContributors[ix][1]]->QxNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][2]]->QxNorm(h+1);
            break;
          }
          case kBCYY: {
            detectorOneValue = newEP_qvec[fDetectorResolutionContributors[ix][1]]->QyNorm(h+1);
            detectorTwoValue = newEP_qvec[fDetectorResolutionContributors[ix][2]]->QyNorm(h+1);
            break;
          }
          }
          fDetectorResolutionCorrelations[ix][iCorr][h]->Fill(values[AliQnCorrectionsVarManager::kVZEROMultPercentile],detectorOneValue*detectorTwoValue);
        }
      }
    }
  }
}  // end loop over events


//__________________________________________________________________
void AnalysisTaskQnVectorAnalysis::FinishTaskOutput()
{
  //
  // Finish Task
  //

  THashList* hList = (THashList*) fEventPlaneHistos->HistList();
  for(Int_t i=0; i<hList->GetEntries(); ++i) {
    THashList* list = (THashList*)hList->At(i);
    fEventQAList->Add(list);
  }

  /* TODO: correct vn with detector resolution */
  for(Int_t iTrkDetector=0; iTrkDetector < nTrackDetectors; iTrkDetector++) {
    for (Int_t iEPDetector=0; iEPDetector < nEPDetectors; iEPDetector++) {
      for(Int_t h=0; h < kNharmonics; h++) {
        for(Int_t corrComp =0; corrComp< kNcorrelationComponents; corrComp++){
          if (fVn[iTrkDetector*nEPDetectors+iEPDetector][h][corrComp]) {
            if (fVn[iTrkDetector*nEPDetectors+iEPDetector][h][corrComp]->GetEntries() > 0)
              fEventQAList->Add(fVn[iTrkDetector*nEPDetectors+iEPDetector][h][corrComp]);
          }
        }
      }
    }
  }

  for (Int_t ix = 0; ix < fNDetectorResolutions; ix++) {
    for (Int_t iCorr = 0; iCorr < nCorrelationPerDetector; iCorr++) {
      for(Int_t h=0; h < kNharmonics; h++) {
        fEventQAList->Add(fDetectorResolutionCorrelations[ix][iCorr][h]);
      }
    }
  }

  PostData(1, fEventQAList);
}



//__________________________________________________________________
Bool_t AnalysisTaskQnVectorAnalysis::IsEventSelected(Float_t* values) {
  if(!fEventCuts) return kTRUE;
  return fEventCuts->IsSelected(values);
}


