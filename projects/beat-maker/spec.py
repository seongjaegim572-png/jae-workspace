import wave, sys, numpy as np
from PIL import Image, ImageDraw
def load(p, start=0, dur=None):
    w=wave.open(p); sr=w.getframerate(); d=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,2)/32768
    d=d[int(start*sr):int((start+dur)*sr)] if dur else d
    return d.mean(1), sr
def spec_img(p, out, start=0, dur=None, W=1500, H=420):
    x,sr=load(p,start,dur); n=4096; h=max(1,(len(x)-n)//W)
    fr=np.lib.stride_tricks.sliding_window_view(x,n)[::h][:W]*np.hanning(n)
    S=np.abs(np.fft.rfft(fr,axis=1)); f=np.fft.rfftfreq(n,1/sr)
    edges=np.geomspace(30,16000,H+1); img=np.zeros((H,len(S)))
    for i in range(H):
        m=(f>=edges[i])&(f<edges[i+1])
        if not m.any(): m=np.abs(f-edges[i]).argmin()==np.arange(len(f))
        img[H-1-i]=S[:,m].max(1)
    db=20*np.log10(img+1e-6); db=np.clip((db-db.max()+70)/70,0,1)
    r=(np.clip(db*3-1,0,1)*255); g=(np.clip(db*3-2,0,1)*255); b=(np.clip(db*3,0,1)*255*(1-np.clip(db*3-2,0,1)*0.7))
    im=Image.fromarray(np.stack([r,g,b],2).astype('uint8'))
    dr=ImageDraw.Draw(im); secs=len(x)/sr
    for s in range(0,int(secs)+1,2):
        X=int(s/secs*im.width); dr.line([(X,H-8),(X,H)],fill=(255,255,255)); dr.text((X+2,H-20),f"{s}s",fill=(255,255,255))
    for fq in (60,150,500,2000,6000):
        Y=H-1-int(np.searchsorted(edges,fq)); dr.text((2,Y-10),f"{fq}",fill=(0,255,0)); dr.line([(0,Y),(12,Y)],fill=(0,255,0))
    dr.text((60,4),p.split('/')[-1],fill=(255,255,0))
    im.save(out)
for a in sys.argv[1:]:
    parts=a.split(':'); p=parts[0]; st=float(parts[1]) if len(parts)>1 else 0; du=float(parts[2]) if len(parts)>2 else None
    spec_img(p,'img_'+p.split('/')[-1].replace('.wav','')+'.png',st,du)
