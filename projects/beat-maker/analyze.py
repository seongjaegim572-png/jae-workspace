import wave, sys, numpy as np
NOTE=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
MAJ=np.array([6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88]); MIN=np.array([6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17])
def load(p):
    w=wave.open(p); sr=w.getframerate(); d=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,2)/32768; return d,sr
def stft(x,sr,n=2048,h=512):
    win=np.hanning(n); fr=np.lib.stride_tricks.sliding_window_view(x,n)[::h]*win
    return np.abs(np.fft.rfft(fr,axis=1)), np.fft.rfftfreq(n,1/sr), h
def analyze(p, start=None, dur=None):
    d,sr=load(p)
    if start is not None: d=d[int(start*sr):int((start+dur)*sr)]
    m=d.mean(1); S,f,h=stft(m,sr)
    # tempo: onset flux autocorrelation
    flux=np.maximum(np.diff(np.log1p(S*50),axis=0),0).sum(1); flux-=flux.mean()
    ac=np.correlate(flux,flux,'full')[len(flux)-1:]; fps=sr/h
    lags=np.arange(len(ac)); bpm=60*fps/np.maximum(lags,1)
    ok=(bpm>=70)&(bpm<=190); cand=lags[ok]; best=cand[np.argsort(ac[ok])[-3:][::-1]]
    bpms=[round(60*fps/l,1) for l in best]
    # key
    chroma=np.zeros(12); mag=S.mean(0)
    for i,fq in enumerate(f):
        if 60<fq<4000: chroma[int(round(69+12*np.log2(fq/440)))%12]+=mag[i]
    best_k=max(((np.corrcoef(np.roll(prof,k),chroma)[0,1],NOTE[k]+nm) for k in range(12) for prof,nm in((MAJ,' major'),(MIN,' minor'))))
    tot=mag.sum(); band=lambda a,b: mag[(f>=a)&(f<b)].sum()/tot
    rms=np.sqrt((d**2).mean()); peak=np.abs(d).max()
    mid=(d[:,0]+d[:,1])/2; side=(d[:,0]-d[:,1])/2
    geo=np.exp(np.log(S+1e-9).mean(1)); flat=(geo/(S.mean(1)+1e-9)).mean()
    # loudness movement: std of 0.25s rms in dB
    blk=int(sr*0.25); r=[20*np.log10(np.sqrt((m[i:i+blk]**2).mean())+1e-9) for i in range(0,len(m)-blk,blk)]
    # kick/onset density
    lowflux=np.maximum(np.diff(S[:,(f<120)].sum(1)),0); kthr=lowflux.mean()+1.5*lowflux.std()
    kicks=np.sum((lowflux[1:-1]>kthr)&(lowflux[1:-1]>lowflux[:-2])&(lowflux[1:-1]>=lowflux[2:]))
    hiflux=np.maximum(np.diff(S[:,(f>7000)].sum(1)),0); hthr=hiflux.mean()+1.0*hiflux.std()
    hats=np.sum((hiflux[1:-1]>hthr)&(hiflux[1:-1]>hiflux[:-2])&(hiflux[1:-1]>=hiflux[2:]))
    secs=len(m)/sr
    print(f"{p.split('/')[-1][:28]:28} bpm~{bpms} key~{best_k[1]:9} rms {20*np.log10(rms):5.1f}dB crest {20*np.log10(peak/rms):4.1f}dB | sub<60 {band(0,60):.2f} bass60-150 {band(60,150):.2f} lowmid150-500 {band(150,500):.2f} mid500-2k {band(500,2000):.2f} himid2-6k {band(2000,6000):.2f} hi>6k {band(6000,22050):.2f} | width {np.sqrt((side**2).mean())/np.sqrt((mid**2).mean()):.2f} noisiness {flat:.2f} dyn-std {np.std(r):.1f}dB | lowhits/s {kicks/secs:.1f} hihits/s {hats/secs:.1f}")
    return r
for a in sys.argv[1:]:
    if ':' in a: p,s,dd=a.split(':'); analyze(p,float(s),float(dd))
    else: analyze(a)
