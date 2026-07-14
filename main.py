from src.Ponto import Ponto
from src.Vetor import Vetor

def main():
    p = Ponto(1, 2, 3)
    v = Vetor(3, 2, 1)

    print(p)
    print(v)
    print(p + v)

if __name__ == "__main__":
    main()