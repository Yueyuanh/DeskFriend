"""Compare browser references with framebuffer snapshots from real hardware."""
from pathlib import Path
import json,sys,math,statistics
ROOT=Path(__file__).resolve().parents[1]
from PIL import Image,ImageDraw
out=ROOT/'.pio/face_qa'
results=json.loads((out/'results.json').read_text(encoding='utf-8'))
report=[]
for test in results:
    name=test['name'];raw=(out/(name+'_lcd.rgb565')).read_bytes()
    pixels=[]
    for i in range(0,len(raw),2):
        v=raw[i]*256+raw[i+1];r=(v>>11)&31;g=(v>>5)&63;b=v&31
        pixels.append(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)))
    lcd=Image.new('RGB',(240,240));lcd.putdata(pixels);lcd.save(out/(name+'_lcd.png'))
    web=Image.open(out/(name+'_web.png')).convert('RGB');wp=list(web.getdata())
    error=0;large=0;total=0;diff=[]
    for i,(a,b) in enumerate(zip(pixels,wp)):
        delta=[abs(x-y) for x,y in zip(a,b)];diff.append(tuple(min(255,x*3) for x in delta))
        x=i%240+.5-120;y=i//240+.5-120
        if x*x+y*y<119*119:
            error+=sum(delta);large+=max(delta)>32;total+=1
    d=Image.new('RGB',(240,240));d.putdata(diff);d.save(out/(name+'_diff.png'))
    report.append({**{k:v for k,v in test.items() if k!='state'},'mean_channel_error':round(error/total/3,3),'pixels_error_gt32_percent':round(large/total*100,3)})
(out/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
for item in sorted(report,key=lambda x:-x['mean_channel_error'])[:20]:print(item)
for group,start in enumerate(range(0,len(results),16)):
    tests=results[start:start+16];sheet=Image.new('RGB',(960,270*math.ceil(len(tests)/2)), '#263238');draw=ImageDraw.Draw(sheet)
    for i,test in enumerate(tests):
        x=(i%2)*480;y=(i//2)*270;name=test['name'];draw.text((x+5,y+5),name+'   WEB / LCD',fill='white')
        sheet.paste(Image.open(out/(name+'_web.png')),(x,y+25));sheet.paste(Image.open(out/(name+'_lcd.png')),(x+240,y+25))
    sheet.save(out/f'comparison_{group}.jpg')
print('Cases:',len(report),'median channel error:',statistics.median(r['mean_channel_error'] for r in report))
