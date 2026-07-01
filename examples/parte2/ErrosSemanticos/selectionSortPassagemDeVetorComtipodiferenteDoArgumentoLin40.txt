funcao [
	selectionSort (vet[]:int,n:int):int
		[
			i,j, x, Min:int;
		]
		{
			i=1;
			enquanto( i<=n-1) {
				Min=i;
				j=i+1;
				enquanto (j<=n)  {
					se (vet[j] < vet[Min])
					entao
						Min=j;
					fimse;	
					j=j+1;
				}
				x=vet[Min]; vet[Min]=vet[i]; vet[i]=x;
				i=i+1;
			}  
			retorne 1;
		}
]
principal
	[
		vet[10]:car;
		i:int;
	]
	{
		escreva "Digite os valores de um vetor de 10 inteiros ";
		novalinha;
		i=0;
		enquanto (i<10){
			escreva "Digite o valor do elemento ";
			escreva i;
			novalinha;
			leia vet[i];
			i=i+1;
		} 
		selectionSort(vet, 10); /*Erro: passagem de vetor de tipo diferente do argumento da funcao*/
		escreva "Vetor ordenado: ";
		novalinha;
		i=0;
		enquanto (i<10) {
			escreva vet[i];
			escreva " ";
			i=i+1;
		}
	}


