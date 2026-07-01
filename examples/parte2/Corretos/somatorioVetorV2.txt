global [
	vet[10] : int;
	soma: int; /*Soma e variavel global -- Correto*/

]
funcao [
	somaVet(vet[]:int, n:int):int 
		[
			i, soma: int; /*Soma redeclarada como local -- Correto*/
		]
		{
			i=0;
			soma=0;
			enquanto (i<10) {
				soma= soma+vet[i];
				i=i+1;	
			}
			retorne (soma);
		}
]

principal
	[ 
		i: int;
		soma: int;
	]
	{
		i=0;
		enquanto (i<10) {
			leia vet[i];
			i=i+1;
		}
		soma=somaVet(vet,10);
		escreva soma;
	}

