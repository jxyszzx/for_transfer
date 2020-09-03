#include <bits/stdc++.h>
using namespace std;

template <class T> void write(FILE *fout, const T &x) {fwrite(&x, sizeof(T), 1, fout);}
//template <class T> size_t read(FILE *fin, T &x) {return fread(&x, sizeof(T), 1, fin);}

int main(int argc, char ** argv)
{
  if (argc != 3) {
        puts("usage: program in_name[graph.txt] out_name[graph.bin]");
        return 0;
  }
  string in_name = string(argv[1]);
  string out_name = string(argv[2]);
  //FILE *fin = fopen(in_name.c_str(), "r");
  FILE *fout = fopen(out_name.c_str(), "wb");
  freopen(in_name.c_str(), "r", stdin);

  unsigned int u, v;
  while (~scanf("%u%u", &u, &v)) {
        //if (read(fin, u) == 0) break;
        //if (read(fin, v) == 0) break;
        if (u == v) continue;
        write(fout, u);
        write(fout, v);
  }
  return 0;
}