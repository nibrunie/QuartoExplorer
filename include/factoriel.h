
double factoriel(unsigned long long n) {
  if (n <= 2) return n;
  return n * factoriel(n-1);
}

double factoriel_square(unsigned long long n) {
  return factoriel(n) * factoriel(n);
}

