global [
	n: int;
]


funcao[
		fatorial(n:int): int {
			se (n==0)
			entao
				retorne 1;
			senao
				retorne n* fatorial(n-1);
			fimse	
			}
]

principal 
	[
		a:int;
	]
	{
		n =1;
		a=2;	
		enquanto (n>0)  {
       		escreva "digite um numero";
       		novalinha;
       		leia n;    
		}	
		escreva "O fatorial de ";
		escreva n;
		escreva " e: ";
		escreva fatorial(n,a);  /*Erro: chamada de funcao com mais parametros do que os que foram 
							declarados */
		novalinha;
	}

