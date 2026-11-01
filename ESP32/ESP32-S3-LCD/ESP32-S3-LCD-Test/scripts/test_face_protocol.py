"""Exercise the actual firmware parser over HTTP; no mocked implementation."""
import json,sys,urllib.request,urllib.error
base=sys.argv[1] if len(sys.argv)>1 else 'http://192.168.137.73'
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def post(body):
    if isinstance(body,dict):body=json.dumps(body,ensure_ascii=False).encode()
    request=urllib.request.Request(base+'/api/face',body,{'Content-Type':'application/json'})
    try:
        with opener.open(request,timeout=5) as response:return response.status,response.read()
    except urllib.error.HTTPError as error:return error.code,error.read()
good={'protocol':2,'seq':30000,'face':'idle','time':1,'age':1}
tests=[
    ('valid state',good,200),
    ('malformed',b'{"face":',400),
    ('trailing data',b'{"face":"idle"}xx',400),
    ('unknown face',{**good,'face':'not-a-face'},400),
    ('negative progress',{**good,'holdProgress':-.1},400),
    ('excess progress',{**good,'holdProgress':1.1},400),
    ('string progress',{**good,'holdProgress':'0.5'},400),
    ('wrong boolean',{**good,'blink':'true'},400),
    ('overlong UTF8 caption',{**good,'text':'浩'*100},400),
    ('too many chat pages',{**good,'face':'chat','chat':{'pages':[['x']]*5,'starts':[0,1,2,3,4]}},400),
    ('empty page',{**good,'face':'chat','chat':{'pages':[[]],'starts':[0]}},400),
    ('infinite float after cast',{**good,'face':'chat','chat':{'pages':[['x']],'starts':[1e200]}},400),
    ('bad camera joints',{**good,'face':'stick','stick':{'camera':True,'joints':[[0,0]]}},400),
    ('oversize body',b' '*4096,400),
]
for name,body,expected in tests:
    status,response=post(body)
    assert status==expected,(name,status,response)
    print('PASS',name,status)
post({'protocol':2,'seq':30001,'face':'idle','time':0,'age':0})
print('Protocol checks:',len(tests),'passed')
