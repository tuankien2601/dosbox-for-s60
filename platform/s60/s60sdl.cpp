/*
    SDL - Simple DirectMedia Layer

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


*/



#include <coecntrl.h>
#include <aknappui.h>
#include <aknapp.h>
#include <akndoc.h>
#include <sdlepocapi.h>
#include <aknnotewrappers.h>
#include <eikstart.h>

const TUid KUidSdlApp={ 0xA00042DD };

TInt Ticks;
TInt Frames;
TInt Done;

class MExitWait
 	{
 	public:
 		virtual void DoExit(TInt aErr) = 0;
 	};   

class CExitWait : public CActive
 	{
 	public:
 		CExitWait(MExitWait& aWait);
 		void Start();
 		~CExitWait();
 	private:
 		void RunL();
 		void DoCancel();
 	private:
 		MExitWait& iWait;
 		TRequestStatus* iStatusPtr;
 	};

class CSDLWin : public CCoeControl
	{
	public:
		void ConstructL(const TRect& aRect);
		RWindow& GetWindow() const;
		void SetNoDraw();
	private:
		void Draw(const TRect& aRect) const;
	}; 	
	
class CSdlApplication : public CAknApplication
	{
private:
	// from CApaApplication
	CApaDocument* CreateDocumentL();
	TUid AppDllUid() const;
	};
	
	
class CSdlAppDocument : public CAknDocument
	{
public:
	CSdlAppDocument(CEikApplication& aApp): CAknDocument(aApp) { }
private:
	CEikAppUi* CreateAppUiL();
	};
	
		
class CSdlAppUi : public CAknAppUi, public MExitWait
    {
public:
    void ConstructL();
    ~CSdlAppUi();
private:
	void HandleCommandL(TInt aCommand);
    void StartTestL(TInt aCmd);
    void DoExit(TInt aErr);
    void HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination);
private:
	CExitWait* iWait;
	CSDLWin* iSDLWin;
	CSDL* iSdl;
	TBool iExit;
  	};	


CExitWait::CExitWait(MExitWait& aWait) : CActive(CActive::EPriorityStandard), iWait(aWait)
	{
	CActiveScheduler::Add(this);
	}
	
CExitWait::~CExitWait()
	{
	Cancel();
	}
 
void CExitWait::RunL()
	{
	if(iStatusPtr != NULL )
		iWait.DoExit(iStatus.Int());
	}
	
void CExitWait::DoCancel()
	{
	if(iStatusPtr != NULL )
		User::RequestComplete(iStatusPtr , KErrCancel);
	}
	
void CExitWait::Start()
	{
	SetActive();
	iStatusPtr = &iStatus;
	}

void CSDLWin:: ConstructL(const TRect& aRect)	
	{
	CreateWindowL();
	SetRect(aRect);
	ActivateL();
	}

	
RWindow& CSDLWin::GetWindow() const
	{
	return Window();
	}
	

void CSDLWin::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(KRgbGray);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(0xaaaaaa);
	gc.DrawRect(Rect());
	}	
	
void CSdlAppUi::ConstructL()	
	{
	BaseConstructL(CAknAppUi::EAknEnableSkin /*| ENoScreenFurniture*/ );
	//TAppUiOrientation currentOrientation = Orientation();
	//if( currentOrientation == EAppUiOrientationPortrait )
	SetOrientationL(EAppUiOrientationLandscape);
	
	iSDLWin = new (ELeave) CSDLWin;
 	iSDLWin->ConstructL(ApplicationRect());
 	
 	
  	iWait = new (ELeave) CExitWait(*this); 
  	StartTestL(0);
	}

void CSdlAppUi::HandleCommandL(TInt aCommand)
	{
	//SetOrientationL(EAppUiOrientationLandscape);
	switch(aCommand)
		{
		case EAknCmdExit:
		case EAknSoftkeyExit:
		case EEikCmdExit:
        	Done = 1;
     		iExit = ETrue;
     		DoExit(KErrNone);
  			//if(iWait == NULL || !iWait->IsActive())
  			//	Exit();	
			break;
    	default:
			if(iSdl == NULL)
				StartTestL(aCommand);
		}
	}

extern TInt GetS60ScaleOption();
TInt s60scale;

void CSdlAppUi::StartTestL(TInt aCmd)
	{
  	TInt flags = CSDL::ENoFlags;
  	s60scale = GetS60ScaleOption();
  	if(s60scale==1)
  		flags |= CSDL::EAllowImageResize | CSDL::EImageResizeZoomOut;
  	if(s60scale==2)
  		flags |= CSDL::EAllowImageResize | CSDL::EImageResizeZoomOut | CSDL::EAllowImageResizeKeepRatio;
  	
  	//switch(aCmd)
  		
		/*case ETestGdi:*/ 
			//flags |= CSDL::EDrawModeGdi;
  		/*case ETestDsa:*/ 
			//  ---> default flags |= CSDL::ENoFlags;
  		/*case ETestDsbDb:*/ 
			//flags |= CSDL::EDrawModeDSBDoubleBuffer; 
  		/*case ETestDsbIu:*/ 
			//flags |= CSDL::EDrawModeDSBIncrementalUpdate; 
  		/*case ETestDsbDbA:*/ 
			//flags |= (CSDL::EDrawModeDSBDoubleBuffer | CSDL::EDrawModeDSBAsync); 
  		/*case ETestDsbIuA:*/
			//flags |= (CSDL::EDrawModeDSBIncrementalUpdate | CSDL::EDrawModeDSBAsync);
  		
  	
  	iSdl = CSDL::NewL(flags);
 	
 	iSdl->SetContainerWindowL(
 					iSDLWin->GetWindow(), 
        			iEikonEnv->WsSession(),
        			*iEikonEnv->ScreenDevice());
    Done = 0;
  	iSdl->CallMainL(iWait->iStatus);    
  	iWait->Start();
	}
    
void CSdlAppUi::DoExit(TInt aErr)
	{
	/*if(aErr != KErrNone)
		{
		CAknErrorNote* err = new (ELeave) CAknErrorNote(ETrue);
		TBuf<64> buf;
		buf.Format(_L("SDL Error %d"), aErr);
		err->ExecuteLD(buf);
		}	
	else
		{
		CAknInformationNote* info = new (ELeave) CAknInformationNote(ETrue);
		info->SetTimeout(CAknNoteDialog::ENoTimeout);
		TBuf<64> buf;
		const TReal ticks = TReal(Ticks) / 1000.0;
		const TReal fps = TReal(Frames) / ticks;
		buf.Format(_L("Fps %f, %dms %d frames"), fps, Ticks, Frames);
		info->ExecuteLD(buf);
		}*/
	delete iSdl;
	iSdl = NULL;
	
	if(iExit)
		Exit();
	}
	
void CSdlAppUi::HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination)
 	{
 	if(iSdl != NULL)
 		iSdl->AppendWsEvent(aEvent);
 	CAknAppUi::HandleWsEventL(aEvent, aDestination);
 	}
 		
CSdlAppUi::~CSdlAppUi()
	{
	if(iWait != NULL)
		iWait->Cancel();
	delete iSdl;
	delete iWait;
	delete iSDLWin;
	}

CEikAppUi* CSdlAppDocument::CreateAppUiL()
    {
    return new(ELeave) CSdlAppUi();
    }	
	
TUid CSdlApplication::AppDllUid() const
    {
    return KUidSdlApp;
    }	
    

CApaDocument* CSdlApplication::CreateDocumentL()
    {
    CSdlAppDocument* document = new (ELeave) CSdlAppDocument(*this);
    return document;
    }
  
LOCAL_C CApaApplication* NewApplication()
    {
    return new CSdlApplication;
    }

GLDEF_C TInt E32Main()
    {
    return EikStart::RunApplication(NewApplication);
    }
    



/*
#include<eikstart.h>
#include<sdlmain.h>
#include<sdlepocapi.h>

GLREF_C TInt E32Main()
    {
    return SDLEnv::SetMain(SDL_main, 
				CSDL::EEnableFocusStop 
				// | CSDL::EAllowImageResize
				,NULL, 
				// SDLEnv::EParamQuery 
				// | SDLEnv::EVirtualMouse | 
				SDLEnv::EFastZoomBlitter
				);
    }
*/    
    
