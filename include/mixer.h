/*
 *  Copyright (C) 2002-2007  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DOSBOX_MIXER_H
#define DOSBOX_MIXER_H

#ifndef C_NOAUDIO

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

typedef void (*MIXER_MixHandler)(Bit8u * sampdate,Bit32u len);
typedef void (*MIXER_Handler)(Bitu len);

enum BlahModes {
	MIXER_8MONO,MIXER_8STEREO,
	MIXER_16MONO,MIXER_16STEREO
};

enum MixerModes {
	M_8M,M_8S,
	M_16M,M_16S,
};

#define MIXER_BUFSIZE (16*1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE-1)
extern Bit8u MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

#define MIXER_SSIZE 4
#define MIXER_SHIFT 14
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_VOLSHIFT 13

struct MIXER_Channel {
	double vol_main[2];
	Bits vol_mul[2];
	Bit8u mode;
	Bitu freq;
	char * name;
	MIXER_MixHandler handler;
	Bitu sample_add;
	Bitu sample_left;
	Bitu remain;
	bool playing;
	MIXER_Channel * next;
};

class MixerChannel;

struct _smixer {
	Bit32s work[MIXER_BUFSIZE][2];
	Bitu pos,done;
	Bitu needed, min_needed, max_needed;
	Bit32u tick_add,tick_remain;
	float mastervol[2];
	MixerChannel * channels;
	bool nosound;
	Bit32u freq;
	Bit32u blocksize;
};

static _smixer mixer;

class MixerChannel {
public:
	void SetVolume(float _left,float _right);
	void UpdateVolume(void);
	void SetFreq(Bitu _freq);
	void Mix(Bitu _needed);
	void AddSilence(void);			//Fill up until needed
	template<bool _8bits,bool stereo,bool signeddata>
//INLINE void MixerChannel::AddSamples(Bitu len,void * data) {
	void AddSamples(Bitu len,void * data) {	Bits diff[2];
	Bit8u * data8=(Bit8u*)data;
	Bit8s * data8s=(Bit8s*)data;
	Bit16s * data16=(Bit16s*)data;
	Bit16u * data16u=(Bit16u*)data;
	Bitu mixpos=mixer.pos+done;
	freq_index&=MIXER_REMAIN;
	Bitu pos=0;Bitu new_pos;

	goto thestart;
	while (1) {
		new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			last[0]+=diff[0];
			if (stereo) last[1]+=diff[1];
			pos=new_pos;
thestart:
			if (pos>=len) return;
			if (_8bits) {
				if (!signeddata) {
					if (stereo) {
						diff[0]=(((Bit8s)(data8[pos*2+0] ^ 0x80)) << 8)-last[0];
						diff[1]=(((Bit8s)(data8[pos*2+1] ^ 0x80)) << 8)-last[1];
					} else {
						diff[0]=(((Bit8s)(data8[pos] ^ 0x80)) << 8)-last[0];
					}
				} else {
					if (stereo) {
						diff[0]=(data8s[pos*2+0] << 8)-last[0];
						diff[1]=(data8s[pos*2+1] << 8)-last[1];
					} else {
						diff[0]=(data8s[pos] << 8)-last[0];
					}
				}
			} else {
				if (signeddata) {
					if (stereo) {
						diff[0]=data16[pos*2+0]-last[0];
						diff[1]=data16[pos*2+1]-last[1];
					} else {
						diff[0]=data16[pos]-last[0];
					}
				} else {
					if (stereo) {
						diff[0]=(Bits)data16u[pos*2+0]-32768-last[0];
						diff[1]=(Bits)data16u[pos*2+1]-32768-last[1];
					} else {
						diff[0]=(Bits)data16u[pos]-32768-last[0];
					}
				}
			}
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=freq_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample=last[0]+((diff[0]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		if (stereo) sample=last[1]+((diff[1]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;done++;
	}
}
//template<bool _8bit,bool stereo,bool signeddata>
//	void AddSamples(Bitu len,void * data);
	void AddSamples_m8(Bitu len,Bit8u * data);
	void AddSamples_s8(Bitu len,Bit8u * data);
	void AddSamples_m8s(Bitu len,Bit8s * data);
	void AddSamples_s8s(Bitu len,Bit8s * data);
	void AddSamples_m16(Bitu len,Bit16s * data);
	void AddSamples_s16(Bitu len,Bit16s * data);
	void AddSamples_m16u(Bitu len,Bit16u * data);
	void AddSamples_s16u(Bitu len,Bit16u * data);
	void AddStretched(Bitu len,Bit16s * data);		//Strech block up into needed data
	void FillUp(void);
	void Enable(bool _yesno);
	MIXER_Handler handler;
	float volmain[2];
	Bit32s volmul[2];
	Bitu freq_add,freq_index;
	Bitu done,needed;
	Bits last[2];
	const char * name;
	bool enabled;
	MixerChannel * next;
};

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name);
MixerChannel * MIXER_FindChannel(const char * name);
/* Find the device you want to delete with findchannel "delchan gets deleted" */
void MIXER_DelChannel(MixerChannel* delchan); 

/* Object to maintain a mixerchannel; As all objects it registers itself with create
 * and removes itself when destroyed. */
class MixerObject{
private:
	bool installed;
	char m_name[32];
public:
	MixerObject():installed(false){};
	MixerChannel* Install(MIXER_Handler handler,Bitu freq,const char * name);
	~MixerObject();
};


/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode);
void PCSPEAKER_SetType(Bitu mode);

#endif  // C_NOAUDIO


#endif
