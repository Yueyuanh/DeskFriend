// Full ports of drawChat(), STICKERS/fk/props/drawStick() from the page.
// Included inside face_animation.cpp's private namespace.
void bubble(MiniCanvas& c,float x,float y,float w,float h) {
  c.fillRoundRect(x,y,w,h,12,kChatBubble);
  const float xs[]={x+2,x-5,x+12},ys[]={y+h-10,y+h+2,y+h-2};
  c.fillPolygon(xs,ys,3,kChatBubble);
}
void drawChat(MiniCanvas& c,const FaceUpdate& request,float T,float age) {
  const char* name=request.chatName;
  const float nw=fminf(90,c.measureText(name,15)),sx=120-(32+nw)/2;
  c.fillEllipse(sx+12,42,12,12,kChatAvatar);
  char initial[5]="?";
  if(*name){unsigned char lead=*name;int n=lead<128?1:lead<224?2:lead<240?3:4;memcpy(initial,name,n);initial[n]=0;}
  c.fillText(sx+12,43,initial,14,kWhite,MiniCanvas::kCenter,MiniCanvas::kMiddle);
  c.fillText(sx+32,42,name,15,0x9fb4b8,MiniCanvas::kLeft,MiniCanvas::kMiddle);
  int page=-1;
  for(int i=0;i<request.chatPageCount;++i)if(age>=request.chatStarts[i])page=i;
  if(page<0){
    bubble(c,90,98,60,34);
    for(int i=0;i<3;++i)c.fillEllipse(106+i*14,115-3*fmaxf(0,sinf(T*7-i*1.1f)),3.5f,3.5f,kChatDim);
    return;
  }
  const int count=request.chatPageLines[page];
  float width=0;for(int i=0;i<count;++i)width=fmaxf(width,c.measureText(request.chatLines[page*3+i],18));
  const float bw=fmaxf(72,fminf(156,width+24)),bh=count*24+20,bx=120-bw/2,by=118-bh/2;
  const float p=fminf(1,(age-request.chatStarts[page])/.25f),e=1-powf(1-p,3);
  c.save();c.setAlpha(e);c.translate(0,8*(1-e));bubble(c,bx,by,bw,bh);
  for(int i=0;i<count;++i)c.fillText(bx+12,by+22+i*24,request.chatLines[page*3+i],18,kChatInk,MiniCanvas::kLeft,MiniCanvas::kMiddle);
  char clock[9];snprintf(clock,sizeof(clock),"%02d:%02d",request.clockHour,request.clockMinute);
  c.fillText(bx+bw,by+bh+14,clock,11,kChatDim,MiniCanvas::kRight,MiniCanvas::kMiddle);
  c.restore();
  if(request.chatPageCount>1)for(int i=0;i<request.chatPageCount;++i){
    c.setAlpha(i==page?1:.25f);float r=i==page?3.5f:3;
    c.fillEllipse(120+(i-(request.chatPageCount-1)/2.0f)*12,192,r,r,i==page?kChatBubble:kWhite);
  }
  c.setAlpha(1);
}

struct StickPose {
  float dy=0,lean=0,bow=0;
  float aL[2]={-.15f,-.1f},aR[2]={.15f,.1f},lL[2]={-.08f,0},lR[2]={.08f,0};
  const char* mouth="smile";const char* eyes="open";
};
StickPose stickPose(const char* n,float t) {
  StickPose p;
  if(!strcmp(n,"wave")){float s=sinf(t*9);p.aR[0]=2.5f+.25f*s;p.aR[1]=.7f*s;}
  else if(!strcmp(n,"heart")){p.dy=sinf(t*4)*.6f;p.aL[0]=-2.4f;p.aL[1]=-1.4f;p.aR[0]=2.4f;p.aR[1]=1.4f;}
  else if(!strcmp(n,"cheer")){float s=sinf(t*8);p.dy=-fabsf(s)*1.5f;p.aR[0]=2.9f+.15f*s;p.aR[1]=.2f;p.aL[0]=-.4f;p.aL[1]=-1.3f;p.mouth="open";}
  else if(!strcmp(n,"jack")){float u=(sinf(t*6)+1)/2;p.dy=-2.5f*u;p.aL[0]=-.2f-2.6f*u;p.aL[1]=-.1f;p.aR[0]=.2f+2.6f*u;p.aR[1]=.1f;p.lL[0]=-.06f-.4f*u;p.lR[0]=.06f+.4f*u;p.mouth="open";}
  else if(!strcmp(n,"dance")){float s=sinf(t*6),a=fabsf(s);p.dy=-a*2;p.lean=sinf(t*3)*.35f;p.aL[0]=-2.3f+.5f*s;p.aL[1]=-.5f;p.aR[0]=2.3f+.5f*s;p.aR[1]=.5f;p.lL[0]=-.25f;p.lL[1]=.5f*a;p.lR[0]=.25f;p.lR[1]=.5f*a;p.mouth="open";}
  else if(!strcmp(n,"bow")){p.bow=(sinf(t*2.2f-1.57f)+1)/2;p.aL[0]=-.15f;p.aL[1]=0;p.aR[0]=.15f;p.aR[1]=0;}
  else if(!strcmp(n,"sleep")){p.bow=.8f*(sinf(t*1.5f)+1)/2;p.eyes="closed";p.mouth="o";p.lean=sinf(t*1.5f)*.15f;}
  else if(!strcmp(n,"confused")){p.lean=sinf(t*2)*.3f;p.aR[0]=2.4f;p.aR[1]=1.8f;p.mouth="flat";}
  else if(!strcmp(n,"cry")){p.dy=sinf(t*10)*.4f;p.aL[0]=-2.3f;p.aL[1]=-1.6f;p.aR[0]=2.3f;p.aR[1]=1.6f;p.mouth="frown";}
  else if(!strcmp(n,"shrug")){p.dy=-.8f*fabsf(sinf(t*3));p.aL[0]=-1.4f;p.aL[1]=-.8f;p.aR[0]=1.4f;p.aR[1]=.8f;p.mouth="flat";}
  else if(!strcmp(n,"stretch")){float u=(sinf(t*1.5f-1.57f)+1)/2;p.lean=-.3f*u;p.aL[0]=-.2f-2.9f*u;p.aR[0]=.2f+2.9f*u;p.mouth="o";}
  else if(!strcmp(n,"run")){float s=sinf(t*10);p.dy=-fabsf(s)*1.5f;p.aL[0]=-.8f-.5f*s;p.aL[1]=-1.3f;p.aR[0]=.8f-.5f*s;p.aR[1]=1.3f;p.lL[0]=-.15f+.6f*s;p.lL[1]=.8f*fmaxf(0,s);p.lR[0]=.15f-.6f*s;p.lR[1]=.8f*fmaxf(0,-s);p.mouth="open";}
  else p.dy=sinf(t*2)*.5f;
  return p;
}
void drawStick(MiniCanvas& c,const FaceUpdate& req,float t) {
  static uint8_t grid[3600];memset(grid,0,sizeof(grid));
  auto rounded=[](float x){return int(floorf(x+.5f));};
  auto pixel=[&](int x,int y,uint8_t value=255){if(x>=0&&x<60&&y>=0&&y<60)grid[y*60+x]=value;};
  auto dot=[&](float x,float y){int ix=rounded(x)-1,iy=rounded(y)-1;for(int a=0;a<2;++a)for(int b=0;b<2;++b)pixel(ix+a,iy+b);};
  auto seg=[&](const float* a,const float* b){int n=std::max(1,int(ceilf(fmaxf(fabsf(b[0]-a[0]),fabsf(b[1]-a[1])))));for(int i=0;i<=n;++i)dot(a[0]+(b[0]-a[0])*i/n,a[1]+(b[1]-a[1])*i/n);};
  StickPose p=stickPose(req.stick,t);float j[13][2]{};
  if(req.cameraPose){memcpy(j,req.joints,sizeof(j));p.eyes=req.stickEyes;p.mouth=req.stickMouth;}
  else {
    j[4][0]=30+p.lean*3;j[4][1]=34+p.dy;
    j[1][0]=j[4][0]+p.lean*5;j[1][1]=j[4][1]-12+5*p.bow;
    j[0][0]=j[1][0]+p.lean*2;j[0][1]=j[1][1]-6+2*p.bow;
    j[2][0]=j[1][0]-3;j[2][1]=j[1][1]+1;j[3][0]=j[1][0]+3;j[3][1]=j[1][1]+1;
    auto polar=[](float x,float y,float a,float l,float* out){out[0]=x+l*sinf(a);out[1]=y+l*cosf(a);};
    polar(j[2][0],j[2][1],p.aL[0],7,j[5]);polar(j[3][0],j[3][1],p.aR[0],7,j[6]);
    polar(j[4][0]-2,j[4][1],p.lL[0],9,j[9]);polar(j[4][0]+2,j[4][1],p.lR[0],9,j[10]);
    polar(j[5][0],j[5][1],p.aL[0]+p.aL[1],7,j[7]);polar(j[6][0],j[6][1],p.aR[0]+p.aR[1],7,j[8]);
    polar(j[9][0],j[9][1],p.lL[0]+p.lL[1],9,j[11]);polar(j[10][0],j[10][1],p.lR[0]+p.lR[1],9,j[12]);
  }
  for(int x=16;x<45;x+=3){pixel(x,54);pixel(x+1,54);}
  const int links[][2]={{1,4},{2,3},{2,5},{5,7},{3,6},{6,8},{4,9},{9,11},{4,10},{10,12}};
  for(auto& link:links)seg(j[link[0]],j[link[1]]);
  int hx=rounded(j[0][0]),hy=rounded(j[0][1]);float chin[2]={float(hx),float(hy+4)};seg(j[1],chin);
  for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x)if(x*x+y*y<=18)pixel(hx+x,hy+y,0);
  for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x)if(x*x+y*y>=10&&x*x+y*y<=18)pixel(hx+x,hy+y);
  pixel(hx-2,hy-1);pixel(hx+2,hy-1);if(!strcmp(p.eyes,"closed")){pixel(hx-1,hy-1);pixel(hx+1,hy-1);}
  const int smile[][2]={{-2,1},{-1,2},{0,2},{1,2},{2,1}},frown[][2]={{-2,2},{-1,1},{0,1},{1,1},{2,2}},flat[][2]={{-1,2},{0,2},{1,2}},open[][2]={{-1,1},{0,1},{1,1},{-1,2},{0,2},{1,2}},o[][2]={{0,1},{0,2}};
  const int (*mouth)[2]=smile;int count=5;
  if(!strcmp(p.mouth,"frown"))mouth=frown;else if(!strcmp(p.mouth,"flat")){mouth=flat;count=3;}else if(!strcmp(p.mouth,"open")){mouth=open;count=6;}else if(!strcmp(p.mouth,"o")){mouth=o;count=2;}
  for(int i=0;i<count;++i)pixel(hx+mouth[i][0],hy+mouth[i][1]);
  if(!req.cameraPose){
    auto glyph=[&](const char* rows,float x,float y){int ix=rounded(x),iy=rounded(y),xx=0,yy=0;for(const char* r=rows;*r;++r){if(*r=='/'){xx=0;++yy;}else{if(*r=='#')pixel(ix+xx,iy+yy);++xx;}}};
    const char *heart=".#.#./#####/#####/.###./..#..",*star="..#../..#../#####/..#../..#..",*ex="#/#/#/./#",*note=".###/.#.#/.#../##../##..",*z="####/..#./.#../####",*q=".##./#..#/..#./..../..#.",*tear=".#./###/###/.#.",*sweat=".#/##/##";
    const char* n=req.stick;bool bl=int(floorf(t*4))%2;
    if(!strcmp(n,"heart"))for(int i=0;i<3;++i){float a=fmodf(t*.7f+i/3.0f,1);if(a<.85f)glyph(heart,28+(i-1)*4,9-a*6);}
    else if(!strcmp(n,"wave")){if(bl)glyph(star,j[8][0]+2,j[8][1]-6);}
    else if(!strcmp(n,"cheer")){if(bl){glyph(ex,38,8);glyph(ex,21,8);}}
    else if(!strcmp(n,"jack")||!strcmp(n,"stretch")){glyph(star,bl?14:41,14);glyph(star,bl?41:14,24);}
    else if(!strcmp(n,"dance")){float a=fmodf(t*.9f,1);glyph(note,38,22-a*12);glyph(note,15,26-fmodf(a+.5f,1)*12);}
    else if(!strcmp(n,"sleep"))for(int i=0;i<2;++i){float a=fmodf(t*.6f+i*.5f,1);glyph(z,37+a*4,14-a*8);}
    else if(!strcmp(n,"confused"))glyph(q,37,6+sinf(t*6)*.8f);
    else if(!strcmp(n,"cry"))for(int sign:{-1,1}){float y=fmodf(t*9+(sign>0?3:0),10);glyph(tear,j[0][0]+sign*3-1,j[0][1]+1+y);}
    else if(!strcmp(n,"shrug"))glyph(sweat,j[0][0]+6,j[0][1]-4+sinf(t*6)*.5f);
    else if(!strcmp(n,"run"))for(int i=0;i<3;++i){float x=4+fmodf(t*40+i*5,12);for(int px=int(floorf(x));px<int(ceilf(x+5));++px){float coverage=fminf(px+1.0f,x+5)-fmaxf(float(px),x);pixel(px,26+i*7,uint8_t(coverage*255));}}
  }
  c.pixelGrid(grid);
}
