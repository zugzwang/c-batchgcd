#include "utils.hpp"

using namespace std;

// filesPerFloor stores the amount of files in each floor of the tree. Only for
// reading variables from disk without having to scan directories.
vector<unsigned int> filesPerFloor;

/* read_moduli_from_csv allocates and initializes the moduli referenced by
 * input_moduli, from the given file.
 */
void read_moduli_from_csv( \
        string filename, vector<mpz_class> *moduli, vector<int>*IDs) {
    cout << "Reading moduli from " << filename << endl;
    ifstream file(filename);
	string line = "";
	// Iterate through each line and split the content using delimeter
    vector<string> vec;
    mpz_class n;
	while (getline(file, line))
	{
            boost::algorithm::split(vec, line, boost::is_any_of(","));
            n = mpz_class(vec[2]);
            IDs->push_back(stoi(vec[0]));
            moduli->push_back(n);
	}
	// Close the File
	file.close();
    cout << "Done. Read " << moduli->size() << " moduli" << endl;
}

/*
 * product_tree computes the product tree of the input moduli; the leaves
 * contain the input moduli and the root contains their product.
 * Each level is computed and written to disk in a separate folder.
 * This function returns the amount of levels contained in the tree.
 *
 * Warning: Input IS DESTROYED, in order to use the occupied RAM if necessary.
 */
int product_tree(vector<mpz_class> *X) {
    cout << "Computing product tree of " << X->size() << " moduli." << endl;
    vector<mpz_class> current_level, new_level;
    mpz_class *prod = new(mpz_class);
    int l = 0;
    current_level = *X;
    while (current_level.size() > 1) {
        filesPerFloor.push_back(current_level.size());
        write_level_to_file(l, &current_level);

        // Free new level
        vector<mpz_class>().swap(new_level);

        // Multiply
        cout << "   Multiplying " << current_level.size() << " ints of ";
        cout << mpz_sizeinbase(current_level[0].get_mpz_t(), 2) << " bits ";
        cout << endl;
        for (unsigned int i = 0; i < current_level.size()-1; i+=2) {
            *prod = current_level[i] * current_level[i+1];
            new_level.push_back(*prod);
        }

        // Append orphan node
        if (current_level.size()%2 != 0) {
            new_level.push_back(current_level.back());
        }

        current_level = new_level;
        if (l == 0) {
            // Free leaves after using, in order to get that RAM if necessary.
            vector<mpz_class>().swap(*X);
        }
        l ++;
    }
    delete prod;

    // Last floor
    filesPerFloor.push_back(current_level.size());
    write_level_to_file(l, &current_level);

    vector<mpz_class>().swap(current_level);
    vector<mpz_class>().swap(new_level);
    return l+1;
}


/* remainders_squares computes the list remᵢ <- Z mod Xᵢ² where X are the
 * moduli and Z is their product. This list is written to the input address.
 */
void remainders_squares(int levels, vector<mpz_class> *R) {
    mpz_class Z;
    read_level_from_file(0, R);
    read_variable_from_file(levels-1, 0, Z);
    for(unsigned int i = 0; i < R->size(); i++) {
        (*R)[i] *= (*R)[i];
        (*R)[i] = Z % (*R)[i];
    }
}

/* remainders_squares_fast is Bernstein suggestion. It uses more RAM.
 * The temporary vector newR uses the same amount of memory as R, and the
 * internal 'square' needs the double of this amount in the firs iteration
 * (its first value is Z^2).
 * Consequently, the first iteration is the most tense part of the algorithm in
 * terms of memory.
 */
void remainders_squares_fast(int levels, vector<mpz_class> *R) {
    vector<mpz_class> newR;
    read_level_from_file(levels-1, R);
    // Sanity check
    if(int(R->size()) != 1) {
        cout << "Fatal error: Incomplete product tree" << endl;
        throw std::exception();
    }
    for(int l = levels-2; l >= 0; l--) {
        vector<mpz_class>().swap(newR);
        cout << "   Computing partial remainders ";
        cout << levels-2-l << " of " << levels-2 << endl;
        unsigned int lengthY = filesPerFloor[l];
        mpz_class square;
        for(unsigned int i = 0; i < lengthY; i++) {
            read_variable_from_file(l, i, square);
            square *= square;
            square = (*R)[i/2] % square;
            newR.push_back(square);
        }
        *R = newR;
    }
    // Free used memory
    vector<mpz_class>().swap(newR);
}

/*
 * write_level_to_file takes an array of integers stored in the input address
 * and writes them to data/product_level/level<given index>. Each variable is
 * stored in a separate file (see mpz_out_raw).
 */
void write_level_to_file(int l, vector<mpz_class> *X) {
    string dir = "data/product_tree/level" + to_string(l) + "/";
    boost::filesystem::create_directory(dir.c_str());
    cout << "   Writing product tree level to " << dir << " (";
    cout << X->size() << " files)" << endl;
    string filename;
    for(unsigned int i = 0; i < X->size(); i++) {
        filename = dir + to_string(i) + ".gmp";
        FILE* file = fopen(filename.c_str(), "w+");
        mpz_out_raw(file, (*X)[i].get_mpz_t());
        fclose(file);
    }
}

/* read_variable_from_file imports the given raw data, initializes an mpz_class
 * and writes it at the given address.
 */
void read_variable_from_file(int level, int index, mpz_class &x) {
    string dir = "data/product_tree/level" + to_string(level) + "/";
    string filename = dir + to_string(index) + ".gmp";
    mpz_t y;
    mpz_init(y);

    FILE* file = fopen(filename.c_str(), "r");
    mpz_inp_raw(y, file);
    fclose(file);
    x = mpz_class(y);
    mpz_clear(y);
}

/* Similar as read_variable_from_file but for a whole tree level.
 */
void read_level_from_file(int l, vector<mpz_class> *moduli) {
    string dir = "data/product_tree/level" + to_string(l) + "/";
    cout << "   Reading product tree level from " << dir << endl;
    ifstream file(dir);
    vector<mpz_class>().swap(*moduli);
    string filename;
    mpz_t mod;
    mpz_init(mod);

    for(unsigned int i = 0; i < filesPerFloor[l]; i++) {
        filename = dir + to_string(i) + ".gmp";
        FILE* file = fopen(filename.c_str(), "r");
        mpz_inp_raw(mod, file);
        fclose(file);
        moduli->push_back(mpz_class(mod));
    }
    mpz_clear(mod);
    cout << "   ok, read " << moduli->size() << " ints of ";
    cout << mpz_sizeinbase((*moduli)[0].get_mpz_t(), 2) << " bits" << endl;
}
