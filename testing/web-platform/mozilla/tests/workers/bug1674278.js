self.onmessage = async function(e) {
    var a = await self.fetch('2-mib-file.py');
    var b = await a.blob();
    self.close()
    await b.arrayBuffer();
}
