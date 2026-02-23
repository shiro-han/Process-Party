# System-Resource-Monitor
System Resource Monitor Project (made for Rowan Uni's Software Engineering Class)

To execute the c++ files in this repository the user will have to:
	enter 'chmod +x fileName.cpp' in the terminal, to make the file executable.
	make sure g++, the GNU C++ compiler driver is installed, by entering:
		g++ --version
		if none exists, install by:
			sudo apt install build-essentials
	After install, compile by entering:
		g++ -O2 -std=c++17 filename.cpp filename
		
	Examples of current files:

	g++ -O2 -std=c++17 cpu_pct.cpp -o cpu_pct
	chmod +x cpu_pct
	./cpu_pct --interval 1
	./cpu_pct --interval 0.5 --raw

	g++ -O2 -std=c++17 mem_pct.cpp -o mem_cpt
	chmod +x mem_cpt
	./mem_cpt
	./mem_cpt --raw

	g++ -O2 -std=c++17 disk_pct.cpp -o disk_pct
	./disk_pct
	./disk_pct --interval 0.5 --raw

	g++ -O2 -std=c++17 net_rate.cpp -o net_rate
	./net_rate
	./net_rate --interval 0.5 --raw
