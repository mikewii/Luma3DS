import sys
import ftplib
import datetime
from ftplib import FTP

def printf(string):
	print(datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S') + " : " + string);
	

def backupFirm(ftp):
	files = []

	try:
		files = ftp.nlst()
	except ftplib.error_perm, resp:
		if str(resp) != "550 No files found":
			printf("Couldn't retrieve file list")
		else:
			return
	lumaName = 'boot.firm'
	lumaNameB = 'boot.firm.bak'
	for f in files:
		if (f.endswith(lumaNameB)):
			global backup
			backup = 1
			printf("Backup exist")

	for f in files:
		if (f.endswith(lumaName) & (backup == 0)):
			parts = f.split()
			name = parts[len(parts) - 1]
			printf("Creating backup")
			printf("Renaming " + name)
			try:
				ftp.rename(lumaName,lumaNameB)
				printf(name + " was successfully renamed to " + lumaNameB)
			except Exception:
				printf("An error occured")
				continue

def cdTree(ftp, currentDir):
	if currentDir != "":
		try:
			ftp.cwd(currentDir)
		except:
			print "Unexpected error:", sys.exc_info()[0]
			raise

def sendFile(ftp, path, name, file):
	printf("Moving to: ftp:/" + path);
	cdTree(ftp, path)
	backupFirm(ftp)

	try:        
		printf("Sending " + name + " to ftp:" + ftp.pwd())
		ftp.storbinary('STOR '+ name, file);
		file.seek(0, 0)
		printf("Done\n")
	except Exception:
		printf("/!\ An error occured. /!\ ");
	
if __name__ == '__main__':
	print("");
	printf("FTP File Sender\n")
	try:
                global backup
                backup = 0
		filename = sys.argv[1]
		lumaname = 'boot.firm'
		lumapath = '/' + sys.argv[2]
		host = sys.argv[3]
		port = sys.argv[4]
		
		ftp = FTP()
		printf("Connecting to " + host + ":" + port);
		ftp.connect(host, port);
		printf("Connected");
		
		printf("Opening " + filename);
		file = open(sys.argv[1], "rb");
		printf("Success");
	
	except IOError as e:
		printf("/!\ An error occured. /!\ ");
		
	sendFile(ftp, lumapath, lumaname, file)
		
	file.close();

	ftp.quit();
	printf("Disconnected");
