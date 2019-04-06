sc config Browser start= disabled
sc stop Browser

sc config LanmanServer start= disabled
sc stop LanmanServer

netstat -an | findstr :445

shutdwon /r
