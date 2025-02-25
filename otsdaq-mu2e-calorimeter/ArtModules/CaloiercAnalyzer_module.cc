#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include "Offline/RecoDataProducts/inc/CaloDigi.hh"

#include <iomanip>
#include <sstream>
#include <vector>

#include <TDirectory.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TMath.h>
#include <iostream>
#include "TH1.h"
#include "TH2.h"
#include "art_root_io/TFileService.h"

#include <TCanvas.h>
#include <TF1.h>
#include <TLine.h>
#include <TProfile.h>
#include <TSpline.h>
#include <TTree.h>
#include <TVirtualFitter.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>  // Libreria per manipolare il formato di output
#include <iostream>
#include <map>

namespace mu2e
{
class CaloiercAnalyzer : public art::EDAnalyzer
{
  public:
	struct Config
	{
		fhicl::Atom<std::string> caloDigiModuleLabel{
		    fhicl::Name("caloDigiModuleLabel"),
		    fhicl::Comment("caloDigiModuleLabel"),
		    ""};
		fhicl::Atom<std::string> caloDigiInstanceLabel{
		    fhicl::Name("caloDigiInstanceLabel"),
		    fhicl::Comment("caloDigiInstanceLabel"),
		    ""};
		fhicl::Atom<int> verbosity{
		    fhicl::Name("verbosity"), fhicl::Comment("Verbosity [0-2]"), 0};
		fhicl::Atom<std::string> splineFilename{
		    fhicl::Name("splineFilename"), fhicl::Comment("splineFilename"), ""};
		fhicl::Atom<bool> uset0{
		    fhicl::Name("uset0"),
		    fhicl::Comment("Use t0 instead of fitting with templates"),
		    false};
		fhicl::Atom<int> skipAfterN{
		    fhicl::Name("skipAfterN"), fhicl::Comment("Don't fit after N hits"), -1};
	};

	explicit CaloiercAnalyzer(const art::EDAnalyzer::Table<Config>& config);
	void analyze(art::Event const& event) override;
	void endJob() override;

  private:
	std::string caloDigiModuleLabel_;
	std::string caloDigiInstanceLabel_;
	int         verbosity_;
	std::string splineFilename_;
	bool        uset0_;
	int         skipAfterN_;

	std::map<int, std::vector<int>>   digiMap;
	std::map<int, std::vector<float>> timeMap;
	std::map<int, std::vector<float>> chi2Map;
	std::map<int, std::vector<float>> chi2rMap;

	int total_events;
	int last_event;
	int total_hits;
	int total_unsorted;
	int total_badfits;

	art::ServiceHandle<art::TFileService> tfs;
	art::TFileDirectory*                  sameChanDir;
	art::TFileDirectory*                  sameBoardDir;
	art::TFileDirectory*                  diffBoardDir;

	std::map<int, TH1F*>   map_h1_dt_singlechan;
	std::map<int, TGraph*> map_g_dtevt_singlechan;

	std::map<int, TH1F*>   map_h1_dt_sameboard;
	std::map<int, TGraph*> map_g_dtevt_sameboard;

	std::map<int, TH1F*>   map_h1_dt_diffboard;
	std::map<int, TGraph*> map_g_dtevt_diffboard;

	TSpline3* spline;
	TF1*      f_spline;
};
}  // namespace mu2e

mu2e::CaloiercAnalyzer::CaloiercAnalyzer(const art::EDAnalyzer::Table<Config>& config)
    : art::EDAnalyzer{config}
    , caloDigiModuleLabel_(config().caloDigiModuleLabel())
    , caloDigiInstanceLabel_(config().caloDigiInstanceLabel())
    , verbosity_(config().verbosity())
    , splineFilename_(config().splineFilename())
    , uset0_(config().uset0())
    , skipAfterN_(config().skipAfterN())
{
	TFile* filetemp = TFile::Open(splineFilename_.c_str());
	if(!filetemp->IsOpen())
	{
		std::cout << splineFilename_ << " NOT FOUND" << std::endl;
	}

	char hname[200];
	sprintf(hname, "spline_0_1");
	if(filetemp->Get(hname))
		spline = (TSpline3*)filetemp->Get(hname);
	else
	{
		std::cout << "No template.." << std::endl;
	}

	f_spline = new TF1(
	    "f_spline",
	    [this](double* x, double* par) {
		    return par[0] * this->spline->Eval(x[0] - par[1]) + par[2];
	    },
	    0.,
	    19.,
	    3);
	f_spline->SetParNames("scale", "tpeak", "ped");
	f_spline->SetNpx(10000);
	f_spline->SetRange(spline->GetXmin(), spline->GetXmax());

	total_events   = 0;
	total_hits     = 0;
	total_unsorted = 0;
	total_badfits  = 0;
	// TVirtualFitter::SetDefaultFitter("Minuit");

	art::TFileDirectory t_sameChanDir  = tfs->mkdir("sameChannel");
	art::TFileDirectory t_sameBoardDir = tfs->mkdir("sameBoard");
	art::TFileDirectory t_diffBoardDir = tfs->mkdir("diffBoard");
	sameChanDir                        = &t_sameChanDir;
	sameBoardDir                       = &t_sameBoardDir;
	diffBoardDir                       = &t_diffBoardDir;
}

void mu2e::CaloiercAnalyzer::analyze(art::Event const& event)
{
	art::EventNumber_t eventNumber      = event.event();
	int                this_eventNumber = (int)eventNumber;
	total_events++;
	last_event = this_eventNumber;

	const auto& caloDigis =
	    *event.getValidHandle(consumes<mu2e::CaloDigiCollection>(caloDigiModuleLabel_));

	// Fill time-ordered map for each sipm
	digiMap.clear();
	for(uint ihit = 0; ihit < caloDigis.size(); ihit++)
	{
		int   thisID   = caloDigis[ihit].SiPMID();
		float thisTime = caloDigis[ihit].t0();
		// Check if this hit is not the last, if so insert
		bool sorted = true;
		for(uint storedHit = 0; storedHit < digiMap[thisID].size(); storedHit++)
		{
			auto storedDigi = caloDigis[digiMap[thisID][storedHit]];
			if(thisTime < storedDigi.t0())
			{
				std::cout << "Found unsorted hit! (in position " << storedHit << " of "
				          << digiMap[thisID].size() << ") ";
				std::cout << "Current t0: " << thisTime
				          << " , last t0: " << storedDigi.t0() << "\n";
				digiMap[thisID].insert(digiMap[thisID].begin() + storedHit, ihit);
				sorted = false;
				total_unsorted++;
				break;
			}
		}
		if(sorted)
		{  // this triggers for the first one and when all is sorted
			digiMap[thisID].push_back(ihit);
		}
		total_hits++;
	}

	// Check that we filled the map properly
	if(verbosity_ > 0)
	{
		std::cout << "\n-- LIST OF CALODIGIS READ --\n";
		for(auto pair : digiMap)
		{
			int thisID = pair.first;
			std::cout << "SiPM ID " << thisID << " [dtc: " << thisID / 120
			          << ", roc: " << (thisID % 120) / 20 << " (" << (thisID / 20) % 6
			          << "), chan: " << thisID % 20 << "] (" << pair.second.size()
			          << " hits) : ";
			for(int idx : pair.second)
			{
				float thisTime = caloDigis[idx].t0();
				std::cout << thisTime << " ";
			}
			std::cout << "\n";
		}
	}

	// Now, fit the templates and fill a timeMap
	timeMap.clear();
	chi2Map.clear();
	chi2rMap.clear();
	if(uset0_)
	{  // Use t0 instead of fitting
		for(auto pair : digiMap)
		{
			for(int idx : pair.second)
			{
				timeMap[pair.first].push_back(5. * caloDigis[idx].t0());
			}
		}
	}
	else
	{  // Do template fitting
		// TGraphErrors *gadc = new TGraphErrors();
		TGraph* gadc = new TGraph();
		for(auto pair : digiMap)
		{
			for(uint ihit = 0; ihit < pair.second.size(); ihit++)
			{
				if(skipAfterN_ > 0 && int(ihit) >= skipAfterN_)
					continue;                  // stop fitting if surpassed N hits
				uint idx = pair.second[ihit];  // idx is the index in the original
				                               // caloDigis vector
				gadc->Set(0);
				// std::cout<<"Fitting sipm "<<pair.first<<", hit "<<ihit<<", (idx:
				// "<<idx<<")\nWaveform: ";
				auto this_waveform = caloDigis[idx].waveform();
				for(uint gi = 0; gi < this_waveform.size(); gi++)
				{  // Fill the tgraph to be fit
					// std::cout<<this_waveform[gi]<<" ";
					gadc->SetPoint(gi, gi, this_waveform[gi]);
					// gadc->SetPointError(gi, 0., 1.);
				}
				// std::cout<<" ("<<this_waveform.size()<<" points)\n";
				f_spline->SetParameters(3850, 0, 0);
				// double xmin, xmax;
				// f_spline->GetRange(xmin, xmax);
				// std::cout<<"Now fitting in range ("<<xmin<<","<<xmax<<")\n";
				int fitStatus = gadc->Fit("f_spline", "QRN");  // FIT!
				if(fitStatus >= 0)
				{
					// std::cout<<"fitStatus: "<<fitStatus<<", ped:
					// "<<f_spline->GetParameter(2)<<", time:
					// "<<f_spline->GetParameter(1)<<", scale:
					// "<<f_spline->GetParameter(0)<<"\n"; std::cout<<"chi2:
					// "<<f_spline->GetChisquare()<<", ndf: "<<f_spline->GetNDF()<<",
					// chi2/ndf: "<<f_spline->GetChisquare()/f_spline->GetNDF()<<"\n";
					timeMap[pair.first].push_back(
					    5. * (caloDigis[idx].t0() + f_spline->GetParameter(1)));
					chi2Map[pair.first].push_back(f_spline->GetChisquare());
					chi2rMap[pair.first].push_back(f_spline->GetChisquare() /
					                               f_spline->GetNDF());
				}
				else
				{  // if bad fit, don't fill at all
					std::cout << "Bad fit status: " << fitStatus << " for sipmid "
					          << pair.first << " hit " << idx << "\n";
					total_badfits++;
				}
			}
		}
		delete gadc;
	}

	// Print fit results
	if(verbosity_ > 0)
	{
		std::cout << "\n-- LIST OF PULSES FITTED --\n";
		for(auto pair : timeMap)
		{
			std::cout << "SiPMID " << pair.first << " \t: ";
			for(float this_time : pair.second)
			{
				std::cout << this_time << " ";
			}
			std::cout << "\n";
			std::cout << "Chi2 " << pair.first << " \t: ";
			for(float this_chi2 : chi2Map[pair.first])
			{
				std::cout << this_chi2 << " ";
			}
			std::cout << "\n";
			std::cout << "Chi2r " << pair.first << " \t: ";
			for(float this_chi2 : chi2rMap[pair.first])
			{
				std::cout << this_chi2 << " ";
			}
			std::cout << "\n";
		}
	}

	//--------Now we can fill hists

	// Same channel
	for(auto pair : timeMap)
	{
		int thisID = pair.first;
		if(pair.second.size() > 1)
		{  // there must be at least 2 hits
			if(map_h1_dt_singlechan.find(thisID) == map_h1_dt_singlechan.end())
			{  // This hist doesn't exist yet
				TString hname = Form("h1_dt_schan_%d", thisID);
				TString htitle =
				    Form("[SAME CHANNEL] dt between hit 0 and hit 1 of sipm %d", thisID);
				map_h1_dt_singlechan[thisID] =
				    tfs->make<TH1F>(hname, htitle, 600, 10009, 10011);
				map_h1_dt_singlechan[thisID]->GetXaxis()->SetTitle("dt [ns]");
				TString gname = Form("g_dtevt_schan_%d", thisID);
				TString gtitle =
				    Form("[SAME CHANNEL] dt between hit 0 and hit 1 of sipm %d", thisID);
				map_g_dtevt_singlechan[thisID] =
				    tfs->makeAndRegister<TGraph>(gname, gtitle);
				map_g_dtevt_singlechan[thisID]->GetXaxis()->SetTitle("Event number");
				map_g_dtevt_singlechan[thisID]->GetYaxis()->SetTitle("dt [ns]");
				map_g_dtevt_singlechan[thisID]->SetMarkerStyle(20);
			}
			float dt = timeMap[thisID][1] - timeMap[thisID][0];
			map_h1_dt_singlechan[thisID]->Fill(dt);
			map_g_dtevt_singlechan[thisID]->AddPoint(this_eventNumber, dt);
		}
	}

	// Same board
	int previous_DTCROC = -1;
	for(auto pair : timeMap)
	{
		int thisID     = pair.first;
		int thisDTCROC = thisID / 20;
		int thisDTC    = thisDTCROC / 6;
		int thisROC    = thisDTCROC % 6;
		int thisChan   = thisID % 20;
		if(thisDTCROC == previous_DTCROC)
			continue;  // Skip if we are on the same board as previous

		previous_DTCROC = thisDTCROC;
		for(auto nextpair : timeMap)
		{
			int nextID     = nextpair.first;
			int nextDTCROC = nextID / 20;
			int nextChan   = nextID % 20;
			if(thisID == nextID)
				continue;
			if(thisDTCROC == nextDTCROC)
			{  // We found a different channel in the same board
				int idpair = thisID * 10000 + nextID;
				if(map_h1_dt_sameboard.find(idpair) == map_h1_dt_sameboard.end())
				{  // This hist doesn't exist yet
					TString hname  = Form("map_h1_dt_sameboard_%d_%d", nextID, thisID);
					TString htitle = Form(
					    "[SAME BOARD] dt between chan %d and %d (DTC: %d, Board: %d, hit "
					    "0)",
					    nextChan,
					    thisChan,
					    thisDTC,
					    thisROC);
					map_h1_dt_sameboard[idpair] =
					    tfs->make<TH1F>(hname, htitle, 5000, -50, 50);
					map_h1_dt_sameboard[idpair]->GetXaxis()->SetTitle("dt [ns]");
					TString gname  = Form("map_g_dtevt_sameboard_%d_%d", nextID, thisID);
					TString gtitle = Form(
					    "[SAME BOARD] dt between chan %d and %d (DTC: %d, Board: %d, hit "
					    "0)",
					    nextChan,
					    thisChan,
					    thisDTC,
					    thisROC);
					map_g_dtevt_sameboard[idpair] =
					    tfs->makeAndRegister<TGraph>(gname, gtitle);
					map_g_dtevt_sameboard[idpair]->GetXaxis()->SetTitle("Event number");
					map_g_dtevt_sameboard[idpair]->GetYaxis()->SetTitle("dt [ns]");
					map_g_dtevt_sameboard[idpair]->SetMarkerStyle(20);
				}
				float dt = timeMap[nextID][0] - timeMap[thisID][0];
				if(abs(dt) > 5000)
					continue;  // They are not the same hit, discard
				// std::cout<<"Same board, filling evt "<<this_eventNumber<<", thisID:
				// "<<thisID<<", nextID: "<<nextID<<", dt = "<<dt<<"\n";
				map_h1_dt_sameboard[idpair]->Fill(dt);
				map_g_dtevt_sameboard[idpair]->AddPoint(this_eventNumber, dt);
			}
		}
	}

	// Different board, same chan
	// std::cout<<std::endl;
	for(int chan = 0; chan < 20; chan++)
	{
		int refID = -1;
		for(auto pair : timeMap)
		{
			int thisID     = pair.first;
			int thisDTCROC = thisID / 20;
			int thisDTC    = thisDTCROC / 6;
			int thisROC    = thisDTCROC % 6;
			int thisChan   = thisID % 20;
			// if (chan==1){std::cout<<"thisChan: "<<thisChan<<", thisID: "<<thisID<<",
			// refID: "<<refID<<"\n";}
			if(thisChan == chan)
			{  // Found the channel we want to plot
				if(refID < 0)
				{  // It's the first one, don't do anything
					// if (chan==1) {std::cout<<"refID<0, thisID: "<<thisID<<"\n";}
					refID = thisID;
				}
				else
				{
					// if (chan==1) {std::cout<<"filling event "<<this_eventNumber<<",
					// refID: "<<refID<<", thisID: "<<thisID<<"
					// -------------------------------------\n";}
					int refDTCROC = refID / 20;
					int refDTC    = refDTCROC / 6;
					int refROC    = refDTCROC % 6;
					int idpair    = refID * 10000 + thisID;
					if(map_h1_dt_diffboard.find(idpair) == map_h1_dt_diffboard.end())
					{  // This hist doesn't exist yet
						TString hname = Form(
						    "map_h1_dt_diffboard_chan%d_%d_%d", thisChan, thisID, refID);
						TString htitle = Form(
						    "[DIFFERENT BOARD] dt between (DTC: %d, Board: %d) and (DTC: "
						    "%d, Board: %d) [chan %d, hit 0]",
						    thisDTC,
						    thisROC,
						    refDTC,
						    refROC,
						    thisChan);
						map_h1_dt_diffboard[idpair] =
						    tfs->make<TH1F>(hname, htitle, 20000, -200, 200);
						map_h1_dt_diffboard[idpair]->GetXaxis()->SetTitle("dt [ns]");
						TString gname  = Form("map_g_dtevt_diffboard_chan%d_%d_%d",
                                             thisChan,
                                             thisID,
                                             refID);
						TString gtitle = Form(
						    "[DIFFERENT BOARD] dt between (DTC: %d, Board: %d) and (DTC: "
						    "%d, Board: %d) [chan %d, hit 0]",
						    thisDTC,
						    thisROC,
						    refDTC,
						    refROC,
						    thisChan);
						map_g_dtevt_diffboard[idpair] =
						    tfs->makeAndRegister<TGraph>(gname, gtitle);
						map_g_dtevt_diffboard[idpair]->GetXaxis()->SetTitle(
						    "Event number");
						map_g_dtevt_diffboard[idpair]->GetYaxis()->SetTitle("dt [ns]");
						map_g_dtevt_diffboard[idpair]->SetMarkerStyle(20);
					}
					float dt = timeMap[thisID][0] - timeMap[refID][0];
					if(abs(dt) > 5000)
						continue;  // They are not the same hit, discard
					map_h1_dt_diffboard[idpair]->Fill(dt);
					map_g_dtevt_diffboard[idpair]->AddPoint(this_eventNumber, dt);
				}
			}
		}
	}
}

void mu2e::CaloiercAnalyzer::endJob()
{
	std::cout << "\n-- JOB SUMMARY --\n";
	std::cout << "Total events: " << total_events << " (last event: " << last_event
	          << ")\n";
	std::cout << "Total hits: " << total_hits << "\n";
	std::cout << "Total unsorted hits found: " << total_unsorted << "\n";
	std::cout << "Total bad fits: " << total_badfits << "\n";
}

DEFINE_ART_MODULE(mu2e::CaloiercAnalyzer)
