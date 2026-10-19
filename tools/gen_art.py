# Generates meta/icon.png (48x48) and meta/banner.png (256x128). The banner sound is tools/gen_banner_wav.py
# Cyberpunk HUD style: carbon black, acid yellow #FCEE0A accent, small red alert.
import math, struct, wave, random, sys
from PIL import Image, ImageDraw, ImageFont
CJK_B = "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
BG=(8,8,10); PANEL=(16,16,19); LINE=(58,58,66); DIM=(120,120,128); TXT=(230,230,224)
ACC=(252,238,10); RED=(255,0,60)

def chamfer(d, box, cut, outline=None, fill=None, width=1, corners=("tl","br")):
    x0,y0,x1,y1=box; pts=[]
    pts += [(x0,y0+cut),(x0+cut,y0)] if "tl" in corners else [(x0,y0)]
    pts += [(x1-cut,y0),(x1,y0+cut)] if "tr" in corners else [(x1,y0)]
    pts += [(x1,y1-cut),(x1-cut,y1)] if "br" in corners else [(x1,y1)]
    pts += [(x0+cut,y1),(x0,y1-cut)] if "bl" in corners else [(x0,y1)]
    d.polygon(pts, fill=fill, outline=outline, width=width)

def dots(d,W,H,S,step):
    for y in range(step//2*S, H*S, step*S):
        for x in range(step//2*S, W*S, step*S): d.rectangle([x,y,x+S-1,y+S-1], fill=(40,40,46))

def scan(im,S):
    d=ImageDraw.Draw(im,"RGBA")
    for y in range(0,im.size[1],2*S): d.rectangle([0,y,im.size[0],y+S-1], fill=(0,0,0,40))

def scope(d,x0,x1,cy,amp,S,seed=7,col=ACC,w=1):
    random.seed(seed); pts=[]
    n=90
    for i in range(n):
        t=i/(n-1); env=math.sin(math.pi*t)**1.5
        v=(math.sin(t*31)+0.6*math.sin(t*77+1)+0.3*(random.random()-0.5))*env
        pts.append((x0+(x1-x0)*t, cy-v*amp))
    d.line(pts, fill=col, width=w*S)

# ---------- icon 48x48 (render x8)
S=8; W=H=48
im=Image.new("RGB",(W*S,H*S),BG); d=ImageDraw.Draw(im)
dots(d,W,H,S,6)
chamfer(d,(1*S,1*S,47*S-1,47*S-1),8*S,outline=ACC,width=S)
# icon: katakana + red tag (v2 design), no radio wave; glyph optically centred by its pixel bbox
font=ImageFont.truetype(CJK_B,32*S)
lay=Image.new("L",(W*S,H*S),0); ImageDraw.Draw(lay).text((0,0),"ラ",font=font,fill=255)
bx0,by0,bx1,by1=lay.getbbox()
gw,gh=bx1-bx0,by1-by0
cx,cy=24*S,24*S                       # centre of the 1..47 frame
ox=cx-gw/2-bx0; oy=cy-gh/2-by0
d.text((ox,oy),"ラ",font=font,fill=ACC)
print("glyph bbox px (48 scale): w=%.1f h=%.1f top=%.2f bottom=%.2f"%(gw/S,gh/S,(oy+by0)/S,(oy+by1)/S))
d.rectangle([33*S,4*S,43*S,8*S],fill=RED)
scan(im,S)
im.resize((48,48),Image.LANCZOS).save("meta/icon.png")
if "--icon-only" in sys.argv:
    print("icon ok"); sys.exit(0)

# ---------- banner 256x128 (render x4)
S=4; W,H=256,128
im=Image.new("RGB",(W*S,H*S),BG); d=ImageDraw.Draw(im)
dots(d,W,H,S,8)
chamfer(d,(4*S,4*S,252*S,124*S),12*S,outline=LINE,width=S)
d.line([(4*S,16*S),(16*S,4*S)],fill=ACC,width=2*S); d.line([(240*S,124*S),(252*S,112*S)],fill=ACC,width=2*S)
mono=ImageFont.truetype(MONO,7*S)
d.text((14*S,10*S),"RADIO//NEON  V1",font=mono,fill=ACC)
d.text((242*S,10*S),"JP-NET 0x3A7F",font=mono,fill=DIM,anchor="ra")
d.line([(12*S,20*S),(244*S,20*S)],fill=LINE,width=S); d.line([(12*S,20*S),(40*S,20*S)],fill=ACC,width=S)
d.text((14*S,24*S),"RADIO NEON",font=ImageFont.truetype(CJK_B,30*S),fill=TXT)
d.text((15*S,64*S),"ラジオネオン",font=ImageFont.truetype(CJK_B,15*S),fill=ACC)
chamfer(d,(150*S,66*S,198*S,80*S),4*S,fill=RED)
d.text((174*S,73*S),"ON AIR",font=mono,fill=TXT,anchor="mm")
chamfer(d,(14*S,88*S,242*S,116*S),6*S,outline=LINE,width=S)
scope(d,18*S,238*S,102*S,9*S,S,w=1)
d.text((240*S,90*S),"SCOPE",font=ImageFont.truetype(MONO,5*S),fill=DIM,anchor="ra")
d.text((246*S,68*S),"無線",font=ImageFont.truetype(CJK_B,22*S),fill=(48,46,8),anchor="ra")
scan(im,S)
im.resize((W,H),Image.LANCZOS).save("meta/banner.png")

print("ok")
