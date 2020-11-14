//gamess_files.cpp

/*********************************************************************/
/* This source code file is part of PRIMoRDiA software project created 
 * by Igor Barden Grillo at Federal University of Paraíba. 
 * barden.igor@gmail.com ( Personal e-mail ) 
 * igor.grillo@acad.pucrs.br ( Academic e-mail )
 * quantum-chem.pro.br ( group site )
 * IgorChem ( Git Hub account )
 */ 

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
 
/*********************************************************************/
//Including c++ headers
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <fstream>
#include <cmath> // review its need
#include <omp.h>
#include <algorithm>  
//#include <iterator>

//Including PRIMoRDiA headers
//-------------------------------------------------------
#include "../include/log_class.h"
#include "../include/common.h"
#include "../include/Iaorbital.h"
#include "../include/Iatom.h"
#include "../include/Imolecule.h"
#include "../include/Iline.h"
#include "../include/Ibuffer.h"
#include "../include/gamess_files.h"

//-------------------------------------------------------
using std::vector;
using std::string;
using std::stoi;
using std::stod;
using std::endl;
using std::cout;

/**************************************************************/
gamess_files::gamess_files()	:
	molecule( new Imolecule() )	,
	buffer( new Ibuffer() )		, 
	is_open(false)				,
	name_f("noname")			{
}
/**************************************************************/
gamess_files::gamess_files(const char* file_name):
	molecule( new Imolecule() )					,
	buffer( new Ibuffer() )						,
	is_open( false )							,
	name_f(file_name)							{
	
	if ( IF_file( file_name ) ){
		is_open = true;
		if ( !check_file_ext(".log",file_name) ) {
			cout << "Warning! The file has wrong etension name!" << endl;
			m_log->input_message("Warning! The file has wrong etension name!");
			is_open = false;
		}
	}else{
		m_log->input_message("Error opening GAMESS file! Verify its presence in the current directory!");
		cout << "the file named " << file_name << " cannot be opened! " << endl;
	}
}
/**************************************************************/
void gamess_files::parse_log(){	
	m_log->input_message("Starting to parse log file from GAMESS.");
	unsigned int i,j,k,l;
	int aonum		= 0 ;
	int atom_in		= 0;
	int atom_fin	= 0;
	int fmo_in		= 0;
	int fmo_fin		= 0;
	int fmob_in		= 0;
	int fmob_fin	= 0;
	int basis_in	= 0;
	int basis_out	= 0;
	int chg_in		= 0;
	int chg_fin		= 0;
	int noe			= 0;
	int overl_in	= 0;
	int overl_fin	= 0;
	bool solvent	= false;
	bool dftb		= false;
	
	molecule->name = get_file_name( name_f );
	molecule->name = remove_extension( molecule->name.c_str() );
	
	buffer.reset( new Ibuffer(name_f,true) );
	for(i=0;i<buffer->nLines;i++){
		if      ( buffer->lines[i].IF_line("COORDINATES",2,"(BOHR)",3,4) ) atom_in = i;  
		else if ( buffer->lines[i].IF_line("INTERNUCLEAR",0,"(ANGS.)",2,3) ) atom_fin = i;
		else if ( buffer->lines[i].IF_line("ATOMIC",0,"BASIS",1,3)) {
			if ( atom_fin == 0 ) atom_fin = i;
		}  
		else if ( buffer->lines[i].IF_line("SHELL",0,"COEFFICIENT(S)",5,6) ) 
			basis_in = i;
		else if ( buffer->lines[i].IF_line("BASIS",3,"SHELLS",5,8) ){
			if (atom_fin == 0 ) {
				atom_fin  = i;
			}
			basis_out =  i;
		}
		else if ( buffer->lines[i].IF_line("BASIS",4,"FUNCTIONS",5,8) ) aonum = buffer->lines[i].get_int(7);
		else if ( buffer->lines[i].IF_line("PCM",2,"SOLVATION",3,5) ) solvent = true; 
		else if ( buffer->lines[i].IF_line("OVERLAP",0,"MATRIX",1,2) ) overl_in  = i; 
		else if ( buffer->lines[i].IF_line("END",1,"ONE-ELECTRON",3,6) ){
			if ( overl_in > 0 ) overl_fin = i; 
		}
		else if ( buffer->lines[i].IF_line("NCC",0,"HAMILTONIAN",1,2) ){
			if ( overl_in > 0 ) {
				overl_fin = i;
				dftb = true;
			}
		}
		else if ( buffer->lines[i].IF_line("EIGENVECTORS",0,1) )  {
			if (!solvent ){ 
				if ( fmo_in == 0 ) fmo_in  = i;
			}else if ( !molecule->betad ) fmo_in = i;
		}
		else if ( buffer->lines[i].IF_line("BETA",1,"SET",2,4) ) {
			fmob_in = fmo_fin = i;
			molecule->betad = true;
		}
		else if ( buffer->lines[i].IF_line("END",1,"CALCULATION",4,6) ) { 
			if (  fmob_in > 0 ) fmob_fin = i; 
			else fmo_fin = i;
		}
		else if ( buffer->lines[i].IF_line("NUMBER",0,"ELECTRONS",2,5)) noe = buffer->lines[i].pop_int(4);
		else if ( buffer->lines[i].IF_line("MULLIKEN",1,"LOWDIN",3,6) ) chg_in = i;
		else if ( buffer->lines[i].IF_line("BOND",0,"ANALYSIS",4,8) ) chg_fin = i;
		else if ( buffer->lines[i].IF_line("SOLVENT",4,"A.U.",7,8) ) {
			molecule->energy_tot = stod(buffer->lines[i].words[6]);
		}
		else if ( buffer->lines[i].IF_line("TOTAL",0,"ENERGY",1,4) ) {
			molecule->energy_tot = buffer->lines[i].pop_double(3);
		}
	}
	
	buffer.reset( new Ibuffer(name_f,atom_in,atom_fin));
	for(j=1;j<buffer->nLines;j++){
		if ( buffer->lines[j].line_len == 5 ) {
			string symb = buffer->lines[j].words[0];
			double xcrd = buffer->lines[j].pop_double(2); 
			double ycrd = buffer->lines[j].pop_double(2); 
			double zcrd = buffer->lines[j].pop_double(2); 
			molecule->add_atom(xcrd,ycrd,zcrd,symb);
		}
	}
	m_log->input_message("Found number of atoms in the log file: ");
	m_log->input_message( int(molecule->num_of_atoms) );
	
	vector<int> atom_n_basis;
	vector<int> shell_n;
	vector<int> shell_n_size;
	vector<string> shell_t;
	vector<double> exponents;
	vector<double> c_coefficients;
	vector<double> cP_coefficients;
	
	int jj = -1;
	buffer.reset( new Ibuffer(name_f, basis_in,basis_out)  );
	for(i=1;i<buffer->nLines;i++){
		if ( buffer->lines[i].line_len == 5 || buffer->lines[i].line_len == 6 ){
			atom_n_basis.push_back(jj);
			shell_n.push_back( buffer->lines[i].pop_int(0) );
			shell_t.push_back( buffer->lines[i].words[0] );
			exponents.push_back( buffer->lines[i].pop_double(2) );
			if ( buffer->lines[i].line_len == 5 ) 	c_coefficients.push_back( buffer->lines[i].pop_double(2) );
			else if ( buffer->lines[i].line_len == 6 ){
				c_coefficients.push_back( buffer->lines[i].pop_double(2) );
				cP_coefficients.push_back( buffer->lines[i].pop_double(2) );
			}
		}else if ( buffer->lines[i].line_len == 1 ) jj++;
	}
	
	int kk = 1;
	int ii = 0;
	for( i=0;i<shell_n.size();i++){
		if 		( i == shell_n.size()-1  ){
			shell_n_size.push_back(ii);
			if ( shell_n[i] == ++kk )  shell_n_size.push_back(1);
		}
		else if ( shell_n[i] == kk ) ii++;
		else{
			kk++;
			shell_n_size.push_back(ii);
			ii=1;
		}
	}	
	
	buffer.reset(nullptr);
	
	int cont	= 1;
	int rr		= 0;
	int ss	= 0;
	int pp	= 0;
	for( i=0;i<shell_n.size();i++){
		if ( shell_n[i] == cont ){
			if ( shell_t[i] == "S" ){
				Iaorbital orbS;
				for ( j=0;j<shell_n_size[shell_n[i]-1];j++){ orbS.add_primitive( exponents[ss++],c_coefficients[rr++] ); }
				molecule->atoms[atom_n_basis[i]].add_orbital(orbS);			
				cont++;
			}else if ( shell_t[i] == "L" ){
				Iaorbital orbS;
				Iaorbital orbpx, orbpy, orbpz;
				for ( j=0;j<shell_n_size[shell_n[i]-1];j++){
					orbS.add_primitive(exponents[ss],c_coefficients[rr++]);
					orbpz.add_primitive(exponents[ss++],cP_coefficients[pp++]);
				}
				orbpx = orbpy = orbpz;
				orbpx.powx = 1;
				orbpy.powy = 1;
				orbpz.powz = 1;
				orbpx.symmetry = "PX";
				orbpy.symmetry = "PY";
				orbpz.symmetry = "PZ";
				molecule->atoms[atom_n_basis[i]].add_orbital(orbS);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpz);
				cont++;
			}else if ( shell_t[i] == "P" ){
				Iaorbital orbpx, orbpy, orbpz;
				for ( j=0;j<shell_n_size[shell_n[i]-1];j++){
					orbpz.add_primitive(exponents[ss++],c_coefficients[pp++]);
				}
				orbpx = orbpy = orbpz;
				orbpx.powx = 1;
				orbpy.powy = 1;
				orbpz.powz = 1;
				orbpx.symmetry = "PX";
				orbpy.symmetry = "PY";
				orbpz.symmetry = "PZ";
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbpz);
				cont++;
			}else if ( shell_t[i] == "D" ){
				Iaorbital orbdxx, orbdyy, orbdzz, orbdxy, orbdxz, orbdyz;
				for (j=0;j<shell_n_size[shell_n[i]-1];j++){ orbdyz.add_primitive(exponents[ss++],c_coefficients[rr++]); }
				orbdxx = orbdyy =  orbdzz =  orbdxy =  orbdxz =  orbdyz;
				orbdxx.powx = 2;
				orbdyy.powy = 2;
				orbdzz.powz = 2;
				orbdxy.powy = orbdxy.powx = 1;
				orbdxz.powz = orbdxz.powx = 1;
				orbdyz.powz = orbdyz.powy = 1;
				orbdxx.symmetry = "XX";
				orbdyy.symmetry = "YY";
				orbdzz.symmetry = "ZZ";
				orbdxy.symmetry = "XY";
				orbdxz.symmetry = "XZ";
				orbdyz.symmetry = "YZ";
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdxx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdyy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdzz);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdxy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdxz);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbdyz);
				cont++;
			}else if ( shell_t[i] == "F" ){
				Iaorbital orbfxxx, orbfyyy, orbfzzz, orbfxxy, orbfyyx, orbfyyz;
				Iaorbital orbfxxz, orbfzzy, orbfzzx, orbfxyz;
				for (j=0;j<shell_n_size[shell_n[i]-1];j++){ orbfxxx.add_primitive(exponents[ss++],c_coefficients[rr++]); }
				orbfyyy=orbfzzz=orbfxxy=orbfyyx=orbfyyz=orbfxxz=orbfzzy=orbfzzx=orbfxyz=orbfxxx;
				orbfxxx.powx = 3;
				orbfyyy.powy = 3;
				orbfzzz.powz = 3;
				orbfxyz.powx = orbfxxz.powy = orbfxxz.powz = 1;
				orbfxxy.powx = 2;
				orbfxxy.powy = 1;
				orbfyyz.powy = 2;
				orbfyyz.powz = 1;
				orbfyyx.powy = 2;
				orbfyyx.powx = 1;
				orbfxxz.powx = 2;
				orbfxxz.powz = 1;
				orbfzzy.powy = 1;
				orbfzzy.powz = 2;
				orbfzzx.powx = 1;
				orbfzzx.powz = 2;
				orbfxxx.symmetry = "XXX";
				orbfyyy.symmetry = "YYY";
				orbfzzz.symmetry = "ZZZ";
				orbfxyz.symmetry = "XYZ";
				orbfxxy.symmetry = "XXY";
				orbfxxz.symmetry = "XXZ";
				orbfyyx.symmetry = "YYX";
				orbfyyz.symmetry = "YYX";
				orbfzzx.symmetry = "ZZX";
				orbfzzy.symmetry = "ZZY";
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfxxx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfyyy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfzzz);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfxxy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfxxz);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfyyx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfyyz);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfzzx);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfzzy);
				molecule->atoms[atom_n_basis[i]].add_orbital(orbfxyz);
				cont++;
			}
		}
	}
	
	
	if ( dftb ){
		for(i=0;i<molecule->num_of_atoms;i++){
			if ( molecule->atoms[i].element == "H"){
				Iaorbital S;
				molecule->atoms[i].add_orbital(S);
			}else {
				Iaorbital S;
				Iaorbital px,py,pz;
				molecule->atoms[i].add_orbital(S);
				molecule->atoms[i].add_orbital(px);
				molecule->atoms[i].add_orbital(py);
				molecule->atoms[i].add_orbital(pz);
			}
			
		}
	}
	
	vector<int>().swap(atom_n_basis);
	vector<int>().swap(shell_n);
	vector<int>().swap(shell_n_size);
	vector<string>().swap(shell_t);
	vector<double>().swap(exponents);
	vector<double>().swap(cP_coefficients);
	vector<double>().swap(c_coefficients);
		
	int col_n				= 0;
	int row_n 				= 0;
	int col_c 				= 0;
	int line_indicator	= 0;
	
	if ( aonum == 0 ) aonum = molecule->get_ao_number();
	molecule->m_overlap.resize( (aonum*(aonum+1) ) /2 );
	buffer.reset( new Ibuffer(name_f, overl_in,overl_fin) );
	for (i=0;i<buffer->nLines;i++){
		if ( buffer->lines[i].line_len >= 1 && line_indicator == 0 ){
			if ( line_indicator == 0 ) {
				col_n = buffer->lines[i].get_int(0) - 1;
				line_indicator++;
			}
		}else if ( buffer->lines[i].line_len > 4 && line_indicator == 1 ){
			row_n = buffer->lines[i].get_int(0) -1;
			col_c = col_n;
			for(l=0;l<buffer->lines[i].line_len-4;l++){ 
				molecule->m_overlap[col_c + ( row_n*(row_n+1) )/2 ] = buffer->lines[i].pop_double(4);
				//cout << molecule->m_overlap[col_c + ( row_n*(row_n+1) )/2 ]  << " " << col_c <<  " " << row_n  << endl;
				col_c++;
			}
			if ( row_n+1 == aonum ){
				line_indicator = 0;
				if ( col_c == aonum ){
					line_indicator=-1;
				}
			}
		}
	}
	/*
	vector<double>::iterator it;
	for(it=molecule->m_overlap.begin();it<molecule->m_overlap.end();++it){
		cout << *it << endl;
	
	 */
	//-----------------------------------------------------
	// filling the molecular orbitals vector of the molecule
	//-----------------------------------------------------
	col_n 			= 0;
	row_n			= 0;
	col_c				= 0;
	line_indicator= 0;
	molecule->coeff_MO.resize(aonum*aonum);
	if ( molecule->betad ){ molecule->coeff_MO_beta.resize(aonum*aonum);}
	buffer.reset( new Ibuffer(name_f,fmo_in,fmo_fin));
	for( j=1;j<buffer->nLines;j++){	
		if ( buffer->lines[j].line_len > 0 && line_indicator == 0 ) {
			col_n = buffer->lines[j].pop_int(0);
			line_indicator++;
		}
		else if ( buffer->lines[j].line_len > 0 && line_indicator == 1){
			for( k=0;k<buffer->lines[j].line_len;k++){
				molecule->orb_energies.push_back(buffer->lines[j].pop_double(0) );
				molecule->MOnmb++;
			}
			line_indicator++;
		}
		else if ( buffer->lines[j].line_len > 0 && line_indicator == 2 )
			line_indicator = 3;
		else if ( buffer->lines[j].line_len >= 5 && line_indicator == 3 ){
			row_n = buffer->lines[j].pop_int(0);			
			for(l=0;l<buffer->lines[j].line_len-4;l++){ 
				molecule->coeff_MO[(col_n+l-1)*aonum+row_n-1] = buffer->lines[j].pop_double(3);
			}
			if ( row_n == aonum ) line_indicator = 0;
		}
	}
	if ( fmob_in > 0 ){
		buffer.reset( new Ibuffer(name_f,fmob_in,fmob_fin));
		for( j=4;j<buffer->nLines;j++){	
			if ( buffer->lines[j].line_len > 0 && line_indicator == 0 ) {
				col_n = buffer->lines[j].pop_int(0);
				line_indicator++;
			}
			else if ( buffer->lines[j].line_len > 0 && line_indicator == 1 ){
				for( k=0;k<buffer->lines[j].line_len;k++){
					molecule->orb_energies_beta.push_back( buffer->lines[j].pop_double(0) );
					molecule->MOnmb_beta++;
				}
				line_indicator++;
			}
			else if ( buffer->lines[j].line_len >  0 && line_indicator == 2 ) line_indicator = 3;
			else if ( buffer->lines[j].line_len >= 5 && line_indicator == 3 ){
				row_n = buffer->lines[j].pop_int(0);
				for(l=0;l<buffer->lines[j].line_len-4;l++){
					molecule->coeff_MO_beta[(col_n+l-1)*aonum+row_n-1] = buffer->lines[j].pop_double(3);
				}
				if ( row_n == aonum ) line_indicator = 0;
			}
		}
	}
	int cnt_chg = 0;
	buffer.reset( new Ibuffer(name_f,chg_in,chg_fin) );
	for(i=0;i<buffer->nLines;i++){
		if ( buffer->lines[i].line_len == 6 )
			molecule->atoms[cnt_chg++].charge = buffer->lines[i].get_double(3);
	}
	buffer.reset(nullptr);
	
	molecule->energy_tot	= molecule->energy_tot*27.2114;
	molecule->homo_energy= molecule->homo_energy*27.2114; // conversion to electronvolt
	molecule->lumo_energy	= molecule->lumo_energy*27.2114;
	
	molecule->bohr_to_ang();
	for ( i=0;i<molecule->orb_energies.size();i++){molecule->orb_energies[i] *= 27.2114;}
	for ( i=0;i<molecule->orb_energies_beta.size();i++){molecule->orb_energies_beta[i] *= 27.2114;}
	molecule->update();
	molecule->check();
	
	/*
	int cnn =0;
	for(int i =0;i<aonum;i++){
		for (int j=0;j<=i;j++){
			cout << i << " " << j << " " << molecule->m_overlap[cnn++] <<  endl;
 		}
	}
	 */
	m_log->input_message("Total Energy ");
	m_log->input_message(double_to_string(molecule->energy_tot));
	m_log->input_message("HOMO number: ");
	m_log->input_message( int(molecule->homoN) );
	m_log->input_message("HOMO energy: ");
	m_log->input_message(double_to_string(molecule->homo_energy));
	m_log->input_message("LUMO number: ");
	m_log->input_message( int(molecule->lumoN) );
	m_log->input_message("LUMO energy: ");
	m_log->input_message(double_to_string(molecule->lumo_energy));
	m_log->input_message("Atomic orbitals: ");
	m_log->input_message(aonum);
	
	
}
/**************************************************************/
gamess_files::~gamess_files(){}
//================================================================================
//END OF FILE
//================================================================================