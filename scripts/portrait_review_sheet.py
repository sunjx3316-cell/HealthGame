"""Arrange the actual game framebuffers into a face and profile comparison."""
from pathlib import Path
import sys
from PIL import Image,ImageDraw,ImageFont

root,out=map(Path,sys.argv[1:3])
font=ImageFont.truetype("C:/Windows/Fonts/msyh.ttc",22)
title=ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc",30)
sheet=Image.new("RGB",(1152,716),"#13272e")
draw=ImageDraw.Draw(sheet)
draw.text((25,15),"器官保卫战   /   年龄形态实机对照",font=title,fill="#e8eee7")
for i,(prefix,age) in enumerate([('portrait',0),('portrait',2),('portrait',6),('portrait',12),
                                  ('profile',0),('profile',6),('portrait',28),('portrait',86)]):
    im=Image.open(root/f"{prefix}-{age}.png").convert("RGB")
    w,h=im.size
    # All frames retain the camera position used in the runtime capture.
    side=int(h*(.36 if age<=2 else .32 if age<=6 else .295))
    left=w//2-side//2
    pic=im.crop((left,int(h*.025),left+side,int(h*.025)+side)).resize((275,265),Image.Resampling.LANCZOS)
    x=9+(i%4)*286;y=69+(i//4)*317
    sheet.paste(pic,(x,y))
    draw.text((x+8,y+269),f"{age} 岁"+(" · 侧面" if prefix=='profile' else ""),font=font,fill="#bed4cd")
sheet.save(out)

# Optional motion strip. Each frame comes from the application's renderer.
if len(sys.argv)==5:
    motion,destination=map(Path,sys.argv[3:])
    frames=[]
    for i in range(73):
        with Image.open(motion/f"growth-{i:03}.png") as src:
            card=Image.new("RGB",(360,401),"#142a30");card.paste(src.convert("RGB"),(0,0))
        d=ImageDraw.Draw(card)
        d.text((12,365),f"{i/4:g} 岁",font=font,fill="#e5eee9")
        d.rectangle((112,382,346,384),fill="#395155")
        d.rectangle((112,382,112+int(234*i/72),384),fill="#a0cdbe")
        frames.append(card)
    frames[0].save(destination,save_all=True,append_images=frames[1:],loop=0,
                   duration=[550]+[85]*71+[900],disposal=1)
